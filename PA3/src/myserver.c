#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
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
	int status = 0;
	int counter = 0;
	while(1){
		comm_fd = accept(listen_fd, (struct sockaddr*) NULL, NULL);
		pid = fork();

		// Create Child process to deal with new client request
		if (pid == 0){
			close(listen_fd);


			// Stay open until client terminates connection
			char recvbuffer[1024];
			while(1){
				memset(recvbuffer, 0, 1024);
				int recvData = recv(comm_fd, recvbuffer, 1024, 0);
				if(recvData < 0){
					fprintf(stderr, "Error: Failed to recieve\n");
					close (comm_fd);
					exit(1);
				}

				// Check Message content to see client health
				if (strlen(recvbuffer) == 0){
					if (status == 1) ++counter;
					status = 1;
				}
				else{
					counter = 0;
					status = 0;
				}
				if (counter > 10) break;
				if (strcmp(recvbuffer, "exit") == 0) break;

				// Do command
				strcat(recvbuffer, " 2>&1");
				FILE* terminal = popen(recvbuffer, "r");
				if (terminal == NULL){
					fprintf(stderr, "Error: Failed to open terminal\n");
					close (comm_fd);
					exit(1);
				}

				// Get Data
				char line[1024];
				memset(line, 0, 1024);
				int numLines = 0;
				while(fgets(line, sizeof(line), terminal) != NULL){
					++numLines;
				}
				++numLines;

				// Reset terminal
				if (pclose(terminal) == -1){
					fprintf(stderr, "Error: Failed to close terminal\n");
					close (comm_fd);
					exit(1);
				}
				terminal = popen(recvbuffer, "r");
				if (terminal == NULL){
					fprintf(stderr, "Error: Failed to open terminal\n");
					close (comm_fd);
					exit(1);
				}


				char sendBuffer[numLines][1024];
				memset(sendBuffer, 0, sizeof(sendBuffer[0])*numLines);
				int currentPos = 0;
				while(fgets(line, sizeof(line), terminal) != NULL){
					strcpy(sendBuffer[currentPos++], line);
				}
				strcpy(sendBuffer[numLines-1], "EOF-/");


				if (pclose(terminal) == -1){
					fprintf(stderr, "Error: Failed to close terminal\n");
					close (comm_fd);
					exit(1);
				}


				// Send Data
				for(int i = 0; i < numLines; ++i){
					//printf ("Sent %s\n", sendBuffer[i]);
					int sent = send(comm_fd, sendBuffer[i], 1024, 0);
					if (sent < 0){
						fprintf(stderr, "Error: Failed to send data\n");
						close (comm_fd);
						exit(1);
					}

					memset(recvbuffer, 0, 1024);
					recvData = recv(comm_fd, recvbuffer, 1024, 0);
					if(recvData < 0){
						fprintf(stderr, "Error: Failed to recieve\n");
						close (comm_fd);
						exit(1);
					}
					if (strcmp(recvbuffer, "FinalAck") == 0) break;
				}

			}


			close(comm_fd);
			exit(0);
		}



		close(comm_fd);
	}

	close(listen_fd);
}
