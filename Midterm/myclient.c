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
	if (argc < 3){
		fprintf(stderr, "Usage: myclient <IP> <Port> <nfiles>\n");
   	 	return -1;
	}
	char* ip = argv[1];
	char* portString = argv[2];

	// Create and set up Socket connections
	int port = atoi(portString);
	if (port == 0){
		fprintf(stderr, "ERROR: Port not valid\n");
		return -1;
	}

	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof servaddr);
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_fd < 0){
		fprintf(stderr, "ERROR: Socket could not be created\n");
		exit(1);
	}

	int inetCheck = inet_pton(AF_INET, ip, &servaddr.sin_addr);
	if (inetCheck <= 0){
		fprintf(stderr, "ERROR: Invalid address\n");
		close (listen_fd);
		exit(1);
	}


	// Connect to address
	if (connect(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		fprintf(stderr, "ERROR: Connection Failed\n");
		fprintf(stderr, "\t%s\n", strerror(errno));
		close (listen_fd);
		exit(1);
	}
  

	// Read and Send Files:
	for (int i = 3; i < argc; ++i){
		if (access (argv[i], F_OK | R_OK) != 0){
			fprintf(stderr, "ERROR: File %s doesn't exist\n", argv[i]);
			continue;
		}
		FILE* openF = fopen(argv[i], "rb");
		if (openF == NULL){
			fprintf(stderr, "Error: Failed to to open file\n");
			close (listen_fd);
			exit(1);
		}

		char line[1024];
		char buffer[1024];
		memset(buffer, 0, sizeof(buffer));
		while(fgets(line, sizeof(line), openF)){
			strcat(buffer, line);
		}
		char sendData[1024];
		strcpy(sendData, argv[i]);
		strcat(sendData, " ");
		strcat(sendData, buffer);

		if (send(listen_fd, sendData, strlen(sendData), 0) <= 0) {
		    fprintf(stderr, "ERROR: Send failed\n");
		    return -1;
	  	}
	  	sleep(1);

	  	char buffer2[1024];
	  	memset(buffer2, 0, sizeof(buffer2));
		int recvData = recv(listen_fd, buffer2, 1023, 0);
		if (recvData < 0){
			fprintf(stderr, "ERROR: Unable to recieve\n");
			fprintf(stderr, "\t%s\n", strerror(errno));
			exit(1);
  		}
  		sleep(1);
  		char newName[1024];
  		strcpy(newName, argv[i]);
  		strcat(newName, ".echo");
  		FILE* echo = fopen(newName, "wb");
  		if (echo == NULL){
			fprintf(stderr, "Error: Failed to to open file\n");
			close (listen_fd);
			exit(1);
		}
  		fprintf(echo, "%s", buffer2);
  		fclose(echo);
	}

	// End Send
	sleep(1);
	if (send(listen_fd, "EndOfFiles", strlen("EndOfFiles"), 0) <= 0) {
	    fprintf(stderr, "ERROR: Send End failed\n");
	    return -1;
  	}


}