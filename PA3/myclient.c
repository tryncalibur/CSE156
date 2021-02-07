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
#include <readline/readline.h>
#include <readline/history.h>

int main(int argc, char **argv){
	if (argc == 3){
		fprintf(stderr, "Usage: myclient <IP> <Port>\n");
   	 	exit(1);
	}
	char* ip = argv[1];
	char* portString = argv[2];

	// Create and set up Socket connections
	int port = atoi(portString);
	if (port == 0){
		fprintf(stderr, "ERROR: Port not valid\n");
		exit(1);;
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
  
  	// Send line from terminal
  	char* sendbuffer = NULL;
	while(true){
		if (readline(sendbuffer) == NULL) continue;
		int sendData = send(listen_fd, sendbuffer, strlen(sendbuffer), 0)
		if(sendData < 0){
			fprintf(stderr, "Error: Failed to send\n");
			close (listen_fd);
			free(sendbuffer);
			exit(1);
		}

		free(sendbuffer);
		sendbuffer = NULL;
	}

	close (listen_fd);
}