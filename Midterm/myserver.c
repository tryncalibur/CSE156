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
	int comm_fd = accept(listen_fd, (struct sockaddr*) NULL, NULL);


	while(1){
		char buffer[1024];
		memset(buffer, 0 , sizeof(buffer));
		int recvData = recv(comm_fd, buffer, 1023, 0);
		if (recvData < 0){
			fprintf(stderr, "ERROR: Unable to recieve\n");
			fprintf(stderr, "\t%s\n", strerror(errno));
			exit(1);
  		}
  		char* name = strtok(buffer, " ");
  		char* line = strtok(NULL, "");

  		char* getter = strstr(buffer, "EndOfFiles");
  		if (getter != NULL){
  			break;
  		}
  		FILE* create = fopen(name, "wb");
		if (create == NULL){
			fprintf(stderr, "Error: Failed to to open file\n");
			close (listen_fd);
			exit(1);
		}
		fprintf(create, "%s", line);
		fclose(create);
  		if (send(comm_fd, line, strlen(line), 0) <= 0) {
		    fprintf(stderr, "ERROR: ReSend failed\n");
		    close (listen_fd);
			exit(1);
		}  
		sleep(1);

	}

}