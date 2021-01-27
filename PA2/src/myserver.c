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


struct Message{
    char type[10];
    int chunk;
    int chunkTotal;
    int chunklet;
    int chunkletTotal;
    int length;
    char data[1024];
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


int main(int argc, char* argv[]){
	// Check Parameters
	if (argc != 2){
		fprintf(stderr, "Expected Format: ./myserver <port-number>\n");
    	return -1;
	}

	int port = atoi(argv[1]);
	if (port == 0){
		fprintf(stderr, "Error: Enter valid port");
    	return -1;
	}

	// Create SocketFD
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0){
		fprintf(stderr, "Error: Socket Creation failed\n");
    	return -1;
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
    	return -1;
	}


	int res = 0;
    while(res >=0){
      	res = input_timeout(sockfd, 2);

  		if (res > 0){
  			int pid = fork();
  			if (pid < 0){
  				fprintf(stderr, "Error: Failed to fork\n");
    			return -1;
  			}
  			if (pid == 0){
  				// Read Message in child process
  				int len, n;
				struct Message recvMsg;
				len = sizeof(cliaddr);
				n = recvfrom(sockfd, (struct Message*)&recvMsg, sizeof(recvMsg), 0, (struct sockaddr*)&cliaddr, &len);
				if (n < 0){
					fprintf(stderr, "Error: Faild to Recieve Message\n");
    				return -1;
				}

				// Determine Response
				if (strcmp(recvMsg.type, "Check") == 0){
					struct Message confirm;
					char fileName[1024];
					memset(fileName, '\0', sizeof(fileName));
					strcpy(fileName, recvMsg.data);

  					if (access (fileName, F_OK | R_OK) != 0){
  						strcpy(confirm.type, "No");
  					}
  					else{
  						strcpy(confirm.type, "Yes");
  						FILE* f = fopen(fileName, "rb");
  						if (f == NULL){
					    	fprintf(stderr, "Error: Unable to read file\n");
						    return -1;
					  	}
					  	int fileSize = 0;
					  	char line[1024];
					  	while(fgets(line, sizeof(line), f)){
    						fileSize += strlen(line);
  						}
  						confirm.length = fileSize;
  						fclose(f);
  					}
  					int sent = sendto(sockfd, (const struct Message*)&confirm, sizeof(confirm), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
  					if(sent < 0){
  						fprintf(stderr, "Error: Failed to send Confirmation\n");
    					return -1;
  					}
				}
				else if(strcmp(recvMsg.type, "Request") == 0){
					//printf("Recieve Chunk: %d, %d\n", recvMsg.chunk, recvMsg.chunklet);
					struct Message respond;
					strcpy(respond.type, "Chunk");
					respond.chunk = recvMsg.chunk;
					respond.chunkTotal = recvMsg.chunkTotal;
					respond.chunklet = recvMsg.chunklet;
					respond.chunkletTotal = recvMsg.chunkletTotal;

					FILE* readF = fopen(recvMsg.data, "rb");
					if (readF == NULL){
				    	fprintf(stderr, "Error: Unable to read file\n");
					    return -1;
				  	}

				  	int chunkSize = (int) ceil((double) recvMsg.length / recvMsg.chunkTotal);
				  	int position = (chunkSize * recvMsg.chunk);
				  	int chunkletSize = (int) ceil((double)chunkSize / recvMsg.chunkletTotal);
				  	position += (chunkletSize * recvMsg.chunklet);
				  	fseek(readF, position, SEEK_SET);

				  	int currentSizes = chunkletSize;
				  	char results[1024];
				  	char line[1024];
				  	while(fgets(line, sizeof(line), readF)){
				  		int readsize = strlen(line);
				  		if (currentSizes > readsize) {
				  			strcat(results, line);
				  			currentSizes -= readsize;
				  		}
				  		else{
				  			strncat(results, line, currentSizes);
				  			break;
				  		}
				  	}
				  	fclose(readF);
				  	strcpy(respond.data, results);
				  	int sent = sendto(sockfd, (const struct Message*)&respond, sizeof(respond), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
  					if(sent < 0){
  						fprintf(stderr, "Error: Failed to send Confirmation\n");
    					return -1;
  					}

				}
				else{
					printf ("Undefined Type: %s\n", recvMsg.type);
				}
				return 0;
			}
			else continue;
		}	
	}
}