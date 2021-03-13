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

struct Message{
	char fileName[256];
	int sequenceNumber;
	int bufferSize;
	unsigned int buffer[1024];
};

int input_timeout (int fd, unsigned int secs){
	fd_set set;
	struct timeval timeout;

	FD_ZERO (&set);
	FD_SET (fd, &set);

	timeout.tv_sec = secs;
	timeout.tv_usec = 0;
	return(select(FD_SETSIZE, &set, NULL, NULL, &timeout));
}

//---------------------------------------------------------------------------------
int main(int argc, char* argv[]){
	// Check Command line arguements
	if (argc < 4){
		fprintf(stderr, "Usage: ./myclient <IP> <Port> <fileName-1> ... <fileName-n>\n");
		exit(1);
	}

	// Create Socket FD
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0){
		fprintf(stderr, "Error: Unable to create socket\n");
		exit(1);
	}

	// Setup Address
	struct sockaddr_in temp;
	memset(&temp, 0, sizeof(temp));
	temp.sin_family = AF_INET;
	temp.sin_port = htons(atoi(argv[2]));
	if (inet_pton(AF_INET, argv[1], &temp.sin_addr.s_addr) <= 0){
		fprintf(stderr, "Error: <IP> is not accepted\n");
		exit(1);
	}

	// Connect Socket to address
	if (connect(sockfd, (struct sockaddr *)&temp, sizeof(temp)) < 0){
		fprintf(stderr, "Error: Connect Failed\n");
		exit(1);
	}

	// Go through each file
	for(int i = 3; i < argc; ++i){
		// Check if file exists
		if (access (argv[i], F_OK | R_OK) != 0){
			fprintf(stderr, "ERROR: File %s doesn't exist\n", argv[i]);
			continue;
		}

		// Open Files
		FILE* readFile = fopen(argv[i], "rb");
		if(readFile == NULL){
			fprintf(stderr, "Error: Failed to Open File\n");
			close (sockfd);
			exit(1);
		}
		char newName[300];
		strcpy(newName, argv[i]);
		strcat(newName, ".echo");
		FILE* echo = fopen(newName, "wb");
		if(echo == NULL){
			fprintf(stderr, "Error: Failed to Open File\n");
			close (sockfd);
			exit(1);
		}

		// Go through content of file and write
		int seq = 0;
		int eof = 0;
		while(1){
			struct Message sendMsg;
			strcpy(sendMsg.fileName, argv[i]);
			sendMsg.sequenceNumber = seq;

			// Read file
			unsigned int tempRead[1024];
			memset(tempRead, 0, sizeof(tempRead));
			sendMsg.bufferSize = fread(tempRead, 1, sizeof(unsigned int) * 1024, readFile);
			memcpy(sendMsg.buffer, tempRead, sendMsg.bufferSize);

			// Send data segment
			int sent = send(sockfd, (const struct Message*)&sendMsg, sizeof(sendMsg), 0);
			if (sent < 0){
				fprintf(stderr, "Error: Failed to send data\n");
				fclose(readFile);
				fclose(echo);
				close (sockfd);
				exit(1);
			}

			// Wait for response
			int res = 0;
			int counterResend = 0;
			while(res >= 0){
				res = input_timeout(sockfd, 30);

				// Handle Response
				if (res > 0){
					// Get Data
					struct Message recvMsg;
					if (recv(sockfd, &recvMsg, sizeof(recvMsg), 0) < 0){
						fprintf(stderr, "Error: Failed to recieve data\n");
						fclose(readFile);
						fclose(echo);
						close (sockfd);
						exit(1);
					}

					// Correct Data recieved
					if (strcmp(argv[i], recvMsg.fileName) == 0 && seq == recvMsg.sequenceNumber){
						if (recvMsg.bufferSize == 0) {
							eof = 1;
							break;
						}
						fwrite(recvMsg.buffer, 1, recvMsg.bufferSize, echo);
						++seq;
						break;
					}
				}

				// Resend data after 30 seconds
				else if(res == 0){
					++counterResend;
					if (counterResend >= 10){
						res = -5;
						break;
					}
					sent = send(sockfd, (const struct Message*)&sendMsg, sizeof(sendMsg), 0);
					if (sent < 0){
						fprintf(stderr, "Error: Failed to resend data\n");
						fclose(readFile);
						fclose(echo);
						close (sockfd);
						exit(1);
					}
					
					sleep(1);
				}
			}
			// Check if select ended in error
			if (res == -1){
				fprintf(stderr, "Error: Select failed\n");
				close(sockfd);
				fclose(readFile);
				fclose(echo);
				exit(1);
			}
			// Client Timeout
			if (res == -5){
				fprintf(stderr, "Error: Unable to recieve confirm from server\n");
				close(sockfd);
				fclose(readFile);
				fclose(echo);
				exit(1);
			}
			if (eof) break;
		}

		// Close Files
		fclose(readFile);
		fclose(echo);

	}

	close(sockfd);
}