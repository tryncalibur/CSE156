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

// Timeout if no signal
int input_timeout (int fd, unsigned int secs){
	fd_set set;
	struct timeval timeout;

	FD_ZERO (&set);
	FD_SET (fd, &set);

	timeout.tv_sec = secs;
	timeout.tv_usec = 0;
	return(select(FD_SETSIZE, &set, NULL, NULL, &timeout));
}


int main(int argc, char **argv){
	int res = 0;

	// Check Arguements
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

	// Create Socket
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
	int cc = connect(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr));
	if (cc < 0){
		fprintf(stderr, "ERROR: Connection Failed\n");
		fprintf(stderr, "\t%s\n", strerror(errno));
		close (listen_fd);
		exit(1);
	}
  

	// Read and Send Files from command line arguements:
	for (int i = 3; i < argc; ++i){
		// Check if file exist
		if (access (argv[i], F_OK | R_OK) != 0){
			fprintf(stderr, "ERROR: File %s doesn't exist\n", argv[i]);
			continue;
		}

		char buffer[1024];
		memset(buffer, 0, sizeof(buffer));
		strcpy(buffer, argv[i]);

		// Send Filename to server
		if (send(listen_fd, buffer, sizeof(buffer), 0) < 0){
			fprintf(stderr, "Error: Failed to send\n");
			close (listen_fd);
			exit(1);
		}

		//Wait for signal or timeout
		res = 0;
		while(res >=0){
			res = input_timeout(listen_fd, 300);
			if (res > 0) break;
			else{
				fprintf(stderr, "Error: Timeout\n");
				close (listen_fd);
				exit(1);
			}
		}

		// Recieve new Filename from server
		memset(buffer, 0, sizeof(buffer));
		if (recv(listen_fd, buffer, 1024, 0) < 0){
			fprintf(stderr, "Error: Failed to Recieve\n");
			close (listen_fd);
			exit(1);
		}

		// Open File for reading and file for writing
		FILE* fr = fopen(argv[i], "rb");
		FILE* fw = fopen(buffer, "wb");
		if (fr == NULL || fw == NULL){
			fprintf(stderr, "Error: Failed to Open File\n");
			close (listen_fd);
			exit(1);
		}

		// Get Each line from file
		int total = 0;
		unsigned int line[1024];
		while(1){
			// Read line
			memset(line, 0, sizeof(line));
			int sum = fread(line, 1, sizeof(unsigned int) * 1024, fr);
			total += sum;

			// Send data to server
			if (send(listen_fd, line, sizeof(line), 0) < 0){
				fprintf(stderr, "Error: Failed to send\n");
				fclose(fr);
				fclose(fw);
				close (listen_fd);
				exit(1);
			}

			//Wait for signal or timeout
			res = 0;
			while(res >=0){
				res = input_timeout(listen_fd, 300);
				if (res > 0) break;
				else{
					fprintf(stderr, "Error: Timeout\n");
					fclose(fr);
					fclose(fw);
					close (listen_fd);
					exit(1);
				}
			}

			// Recieve data from server
			memset(line, 0, sizeof(line));
			if (recv(listen_fd, line, sizeof(line), 0) < 0){
				fprintf(stderr, "Error: Failed to Recieve\n");
				fclose(fr);
				fclose(fw);
				close (listen_fd);
				exit(1);
			}
			
			// Write To file
			fwrite(line, sizeof(unsigned int), 1024, fw);

			// End loop at end of file
			if (sum == 0 && feof(fr)) break;
		}

		// Close files
		fclose(fr);
		fclose(fw);

		// Send File Ender
		memset(line, 0, sizeof(line));
		memcpy(line, "EOF\n\n", sizeof("EOF\n\n"));
		if (send(listen_fd, line, sizeof(line), 0) < 0){
			fprintf(stderr, "Error: Failed to send\n");
			close (listen_fd);
			exit(1);
		}

		// Prevent combining multiple messages
		sleep(1);

		// Send file length to server
		int size[1] = {total};
		if (send(listen_fd, size, sizeof(size), 0) < 0){
			fprintf(stderr, "Error: Failed to send\n");
			close (listen_fd);
			exit(1);
		}

		// Fix file length of file.echo
		int tt = truncate(buffer, total);
		if (tt == -1){
			fprintf(stderr, "Error: Failed to truncate file\n");
			close (listen_fd);
			exit(1);
		}

	}

	// Send Final Ack
	if (send(listen_fd, "", sizeof(""), 0) < 0){
		fprintf(stderr, "Error: Failed to send\n");
		close (listen_fd);
		exit(1);
	}



	close(listen_fd);
}