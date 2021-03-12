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
	// Check arguements
	if (argc != 2){
		fprintf(stderr, "Expected Format: ./myserver <port-number>\n");
    	exit(1);
	}

	int port = atoi(argv[1]);
	if (port == 0){
		fprintf(stderr, "Error: Enter valid port");
    	exit(1);
	}

	// Setup sockets and connections
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
	


	while(1){
		// Get Connection
		int comm_fd = accept(listen_fd, (struct sockaddr*) NULL, NULL);

		// Loop until all files recieved
		while(1){
			char buffer[1024];
			char curFileName[1024];
			memset(curFileName, 0, sizeof(curFileName));
			memset(buffer, 0, sizeof(buffer));

			// Recieve Filename
			if (recv(comm_fd, buffer, sizeof(buffer), 0) < 0){
				fprintf(stderr, "Error: Failed to Recieve filename or EOF\n");
				close (listen_fd);
				close (comm_fd);
				exit(1);
			}

			// Check if final file has been sent
			if (strcmp(buffer, "") == 0) break;

			// Open File
			FILE* write = fopen(buffer, "wb");
			if (write == NULL){
				fprintf(stderr, "Error: Failed to Open File\n");
				close (listen_fd);
				close (comm_fd);
				exit(1);
			}

			// Save Filename
			strcpy(curFileName, buffer);

			// Send FileName
			strcat(buffer, ".echo");
			if (send(comm_fd, buffer, sizeof(buffer), 0) < 0){
				fprintf(stderr, "Error: Failed to resend file name\n");
				fclose(write);
				close (listen_fd);
				close (comm_fd);
				exit(1);
			}

			// Recieve File lines and send back
			unsigned int bufferi[1024];

			// Define end of file message
			unsigned int empty[1024];
			memset(empty, 0, sizeof(empty));
			memcpy(empty, "EOF\n\n", sizeof("EOF\n\n"));
			
			// Get file content
		  	while(1){
		  		// Recieve Data Lines from client
		  		memset(bufferi, 0, sizeof(bufferi));
				if (recv(comm_fd, bufferi, sizeof(bufferi), 0) < 0){
					fprintf(stderr, "Error: Failed to recieve data\n");
					fclose(write);
					close (listen_fd);
					close (comm_fd);
					exit(1);
				}

				// Check Data to write or end file
				if (memcmp(bufferi, empty, sizeof(bufferi)) == 0) break;
				else fwrite(bufferi, sizeof(unsigned int), 1024, write);

				// Resend Data Lines back to client
				if (send(comm_fd, bufferi, sizeof(bufferi), 0) < 0){
					fprintf(stderr, "Error: Failed to resend data\n");
					fclose(write);
					close (listen_fd);
					close (comm_fd);
					exit(1);
				}
		  	}
		  	// Close File
		  	fclose(write);

		  	// Get file size from client
		  	int size[1];
		  	if (recv(comm_fd, size, sizeof(size), 0) < 0){
				fprintf(stderr, "Error: Failed to recieve file size\n");
				close (listen_fd);
				close (comm_fd);
				exit(1);
			}

			// Truncate file to proper size
			int tt = truncate(curFileName, size[0]);
			if (tt == -1){
				fprintf(stderr, "Error: Failed to truncate file\n");
				close (listen_fd);
				close (comm_fd);
				exit(1);
			}
		}

		close(comm_fd);
	}
	close(listen_fd);
}