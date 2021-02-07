#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include <math.h>
#include <errno.h>

int main(int argc, char **argv){
	if (argc != 2){
		fprintf(stderr, "Expected Format: ./myserver <port-number>\n");
    	exit(1);
	}

	int port = atoi(argv[1]);
	if (port == 0){
		fprintf(stderr, "Error: Enter valid port");
    	exit(1);
	}

	//Connections
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

	// Listen for clients
	int comm_fd, pid;
	while(true){
		comm_fd = accept(listen_fd, (struct sockaddr*) NULL, NULL);
		pid = fork();

		// Create Child process to deal with new client request
		if (pid == 0){
			printf ("Create Child\n");
			close(listen_fd);


			// Stay open until client terminates connection
			char recvbuffer[1024];
			while(true){
				memset(recvbuffer, 0, sizeof(recvbuffer));
				int recvData = recv(comm_fd, recvbuffer, 1023, 0);
				if(recvData < 0){
					fprintf(stderr, "Error: Failed to recieve\n");
					close (comm_fd);
					exit(1);
				}
				if (recvData > 1024){
					fprintf(stderr, "Error: Buffer overflow. Expected max line size of 1024\n");
					continue;
				}
				
				printf ("Recieved: %s\n", recvbuffer);
				if (strcmp(recvbuffer, "Exit") == 0) break;
			}


			close(comm_fd);
			exit(0);
		}



		close(comm_fd);
	}


}