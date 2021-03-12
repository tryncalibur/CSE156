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
	// Check Command Line Arguements
	if (argc != 2){
		fprintf(stderr, "Usage: ./myserver <Port>\n");
		exit(1);
	}

	int port = atoi(argv[1]);
	if (port == 0){
		fprintf(stderr, "Error: Enter valid port");
    	exit(1);
	}

	// Create SocketFD
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0){
		fprintf(stderr, "Error: Socket Creation failed\n");
    	exit(1);
	}

	// Server Info
	struct sockaddr_in servaddr, cliaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	memset(&cliaddr, 0, sizeof(cliaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(port);
	
	// Bind
	if (bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		fprintf(stderr, "Error: Bind Failed\n");
		close (sockfd);
    	exit(1);
	}

	//-------------------------------------------------------------------------------

	// Handle new file
	int recvSize;
	socklen_t len = sizeof(cliaddr);
	char fileName[256];
	memset(fileName, 0 , sizeof(fileName));
	FILE* current = NULL;
	int expectedSeq = 0;
	struct Message tempHold;

	int res = 0;
	while(res >= 0){
		res = input_timeout(sockfd, 5);

		// Recieve Input
		if (res > 0){
			struct Message recvMsg;
			recvSize = recvfrom(sockfd, (struct Message*)&recvMsg, sizeof(recvMsg), 0, (struct sockaddr*)&cliaddr, &len);
			if (recvSize < 0){
				fprintf(stderr, "Error: Failed to Recieve Message\n");
				close (sockfd);
				exit(1);
			}

			// Determine Send Message
			// Create new file
			if (current == NULL && recvMsg.sequenceNumber == 0){
				current = fopen(recvMsg.fileName, "wb");
				if (current == NULL){
					fprintf(stderr, "Error: Failed to Open File\n");
					close (sockfd);
					exit(1);
				}
				strcpy(fileName, recvMsg.fileName);
				expectedSeq = 0;
			}
			// Write to file and send back
			if (current != NULL && expectedSeq == recvMsg.sequenceNumber && strcmp(fileName, recvMsg.fileName) == 0){
				fwrite(recvMsg.buffer, 1, recvMsg.bufferSize, current);
				int sent = sendto(sockfd, (const struct Message*)&recvMsg, sizeof(recvMsg), 0, (const struct sockaddr *)&cliaddr, len);
				if (sent < 0){
					fprintf(stderr, "Error: Failed to Send Message\n");
					close (sockfd);
					exit(1);
				}

				// Increment SeqNum and Keep copy of Sent data
				++expectedSeq;
				tempHold = recvMsg;
			}
			// Resend last message if failed
			else if (strcmp(fileName, recvMsg.fileName) == 0 && recvMsg.sequenceNumber == expectedSeq - 1){
				int sent = sendto(sockfd, (const struct Message*)&tempHold, sizeof(tempHold), 0, (const struct sockaddr *)&cliaddr, len);
				if (sent < 0){
					fprintf(stderr, "Error: Failed to Resend Message\n");
					close (sockfd);
					exit(1);
				}
			}
			// End of file
			if(strcmp(fileName, recvMsg.fileName) == 0 && recvMsg.sequenceNumber == expectedSeq - 1 && recvMsg.bufferSize == 0){
				fclose(current);
				current = NULL;
				expectedSeq = 0;
				memset(fileName, 0 , sizeof(fileName));
			}

		}
	}

	// Check error
	if (res < 0){
		fprintf(stderr, "Error: Select failed\n");
	}

	close(sockfd);
}