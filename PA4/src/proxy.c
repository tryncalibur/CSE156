#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <pthread.h>

// Global Scope
char FSITE_FILENAME[256];
int THREAD_ISSUE;
pthread_mutex_t* THREAD_CHECKER;

// Use to periodically check on socket
int input_timeout (int fd, unsigned int secs){
	fd_set set;
	struct timeval timeout;

	FD_ZERO (&set);
	FD_SET (fd, &set);

	timeout.tv_sec = secs;
	timeout.tv_usec = 0;
	return(select(FD_SETSIZE, &set, NULL, NULL, &timeout));
}

// Check if file is authorized
// -1 = FILE error
//  0 = shares name
//  1 = not shared
int checkSites(char* fsite_name, char* siteName){
	FILE* forbiddenSites = fopen(fsite_name, "rb");
	if (forbiddenSites == NULL){
		fprintf(stderr, "Error: Unable to Open <forbidden-sites-file>\n");
    	return -1;
	}

	// Compare sites
	char line[1024];
	char* rere;
	while(fgets(line, sizeof(line), forbiddenSites)){
		rere = strstr(siteName, line);
		if (rere != NULL) break;
	}
	fclose(forbiddenSites);
	if (rere == NULL) return 1;
	else return 0;
}

// Handle Client's request
void* handleRequest(void* fd){
	char buffer[1024];
	memset(buffer, 0, sizeof(buffer));
	int* sock = (int*)fd;

	// Recieve Data from client
	int recvReq = recv(*sock, buffer, sizeof(buffer), 0);
	if (recvReq < 0){
		fprintf(stderr, "Error: Recv Failed from client\n");
		close(*sock);
		return NULL;
	}

	// Check for issues (mutex for THREAD_ISSUE)
	pthread_mutex_lock(&THREAD_CHECKER);
	if (recvReq == 0) {
		++THREAD_ISSUE;
		pthread_mutex_unlock(&THREAD_CHECKER);
		return NULL;	
	}
	else THREAD_ISSUE = 0;
	pthread_mutex_unlock(&THREAD_CHECKER);

	// Extract Data
	printf("%s", buffer);

	return NULL;
}

//---------------------------------------------------------------------------------------------------
int main(int argc, char **argv){
	memset (FSITE_FILENAME, 0, sizeof(FSITE_FILENAME));
	THREAD_ISSUE = 0;
	if (pthread_mutex_init(&THREAD_CHECKER, NULL) != 0){
		fprintf(stderr, "Error: Failed to init mutex");
    	exit(1);
	}

	// Check Arguements
	if (argc != 3){
		fprintf(stderr, "Expected Format: ./proxy <port-number> <forbidden-sites-file>\n");
    	exit(1);
	}

	// Check if valid Port
	int port = atoi(argv[1]);
	if (port == 0){
		fprintf(stderr, "Error: Enter valid port");
    	exit(1);
	}

	// Check for forbidden Sites in file
	if (access (argv[2], F_OK | R_OK) != 0){
		fprintf(stderr, "ERROR: File %s doesn't exist\n", argv[2]);
		exit(1);
	}
	strcpy(FSITE_FILENAME, argv[2]);


	//-----------------------------------------------------------------------------------------------

	//Create sockets and Connections
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0){
		fprintf(stderr, "Error: Socket Creation failed\n");
    	exit(1);
	}
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(port);
	if (bind(listen_fd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		fprintf(stderr, "Error: Bind Failed\n");
		close (listen_fd);
    	exit(1);
	}
	listen(listen_fd, 500);

	//-----------------------------------------------------------------------------------------------

	// Listen for clients
	int comm_fd;
	while(1){
		comm_fd = accept(listen_fd, (struct sockaddr*) NULL, NULL);
		if (comm_fd < 0){
			fprintf(stderr, "Error: Accept Failed\n");
			close (listen_fd);
			exit(1);
		}
		// Connected with Client
		else{			
			int res = 0;
			int counter = 0;
			while(res >= 0){
				// Check for thread issues
				if (THREAD_ISSUE > 10) {
					fprintf(stderr, "Error: Client no longer sending data\n");
					break;	
				}

				res = input_timeout(comm_fd, 3);

				// Server recieves request
				if (res > 0){
					counter = 0;

					// Handle Request in new thread
					pthread_t newProcess;
					if (pthread_create(&newProcess, NULL, handleRequest, &comm_fd)){
						fprintf(stderr, "Error: Failed to crate a new thread\n");
						close (listen_fd);
						close(comm_fd);
						exit(1);
					}
					sleep(1);

				}
				// Server lacks request
				else{
					printf("---------\nCounter\n---------\n");
					if (++counter > 10) break;
				}
			}
			// Select Failed
			if (res < 0){
				fprintf(stderr, "Error: Select Failed\n");
				close (listen_fd);
				close(comm_fd);
				exit(1);
			}
		}

		close(comm_fd);
	}

	close(listen_fd);
	pthread_mutex_destroy(THREAD_CHECKER);
}
