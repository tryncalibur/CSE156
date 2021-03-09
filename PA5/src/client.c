#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <ifaddrs.h>



// Structs
struct Client{
	char ID[256];
	int fd;
};

struct SendData{
	char ID [256];
	char IP [INET_ADDRSTRLEN];
	int port;
};

// Define Modes
#define QUIT -1
#define INFO 0

#define WAIT_END 1
#define WAIT 2
#define WAIT_SUC 10

#define CHAT_END 3
#define CHAT 4

// GLOBAL SCOPE
char promptID[256];
char GlobalID[256];
int mode;
pthread_mutex_t EDIT_MODE;

// Utility
int input_timeout (int fd, unsigned int secs){
	fd_set set;
	struct timeval timeout;

	FD_ZERO (&set);
	FD_SET (fd, &set);

	timeout.tv_sec = secs;
	timeout.tv_usec = 0;
	return(select(FD_SETSIZE, &set, NULL, NULL, &timeout));
}

// Thread Functions
void* chatRead(void* cData){
	int comm_fd = ((struct Client*)cData)->fd;
	char buffer[1024];
	int res = 0;
	int checkMode = CHAT;

	while(res >= 0){
		// Check Mode
		pthread_mutex_lock(&EDIT_MODE);
		checkMode = mode;
		pthread_mutex_unlock(&EDIT_MODE);
		if (checkMode != CHAT) return NULL;

		res = input_timeout(comm_fd, 0);
		if (res > 0){
			// Get Message
			memset(buffer, 0 , sizeof(buffer));
			int rec = recv(comm_fd, buffer, sizeof(buffer), 0);
			if (rec < 0) {
				fprintf(stderr, "ERROR: Failed to recv from client\n");
				close(comm_fd);
				return NULL;
			}


			// Read input to check if last message
			if (rec > 0){
				if (strcmp(buffer, "END_COMM\n\n") == 0){
					pthread_mutex_lock(&EDIT_MODE);
					--mode;
					pthread_mutex_unlock(&EDIT_MODE);
					write(STDOUT_FILENO, "\n", strlen("\n"));
				}

				// Print message
				else{
					if (checkMode == CHAT){
						write(STDOUT_FILENO, "\n", strlen("\n"));
						write(STDOUT_FILENO, buffer, strlen(buffer));
						write(STDOUT_FILENO, promptID, strlen(promptID));
					}
				}
			}
		}
	}

	close(comm_fd);
 return NULL;
}

void* chatWrite(void* cData){
	int comm_fd = ((struct Client*)cData)->fd;
	char buffer[1024];
	int res = 0;
	int checkMode = CHAT;

	write(STDOUT_FILENO, promptID, strlen(promptID));
	while(res >= 0){
		// Check status
		pthread_mutex_lock(&EDIT_MODE);
		checkMode = mode;
		pthread_mutex_unlock(&EDIT_MODE);
		if (checkMode != CHAT) {
			if (send(comm_fd, "END_COMM\n\n", strlen("END_COMM\n\n"), 0) < 0) {
				fprintf(stderr, "ERROR: Failed to send to client\n");
				close(comm_fd);
				return NULL;
			}
			return NULL;
		}
		
		res = input_timeout(STDIN_FILENO, 0);

		// Read to read
		if (res > 0){
			// Write ID and read input
			memset(buffer, 0, sizeof(buffer));
			if (checkMode == CHAT) read(STDIN_FILENO, buffer, sizeof(buffer));

			char sendbuffer[1300];
			memset(sendbuffer, 0 , sizeof(sendbuffer));

			// Check command
			buffer[strlen(buffer)-1] = '\0';
			if (strcmp(buffer, "/quit") == 0){
				pthread_mutex_lock(&EDIT_MODE);
				mode = QUIT;
				pthread_mutex_unlock(&EDIT_MODE);
				checkMode = QUIT;
			}
			else if(buffer[0] == '/'){
				sprintf(sendbuffer, "Command %s not recognized\n", buffer);
				write(STDOUT_FILENO, sendbuffer, strlen(sendbuffer));
			}

			// Send Message
			else if(strlen(buffer) != 0){
				if (checkMode == CHAT) sprintf(sendbuffer, "%s: %s\n", GlobalID, buffer);
				if (send(comm_fd, sendbuffer, strlen(sendbuffer), 0) < 0) {
					fprintf(stderr, "ERROR: Failed to send to client\n");
					close(comm_fd);
					return NULL;
				}
			}

			if (checkMode == CHAT) write(STDOUT_FILENO, promptID, strlen(promptID));
		}
	}

	//printf("FAILED TO END WRITE THREAD CLEANLY\n");
	return NULL;
}

void* waitWrite(void* v){
	char readTerminal[1024];
	int checkMode = 1;
	int res = 0;
	write(STDOUT_FILENO, promptID, strlen(promptID));
	while(res >= 0){
		// Check status
		pthread_mutex_lock(&EDIT_MODE);
		checkMode = mode;
		pthread_mutex_unlock(&EDIT_MODE);
		if (checkMode != WAIT) return NULL;


		res = input_timeout(STDIN_FILENO, 0);

		// Read to read
		if (res > 0){
			// Write ID and read input
			memset(readTerminal, 0, sizeof(readTerminal));
			if (checkMode == WAIT) read(STDIN_FILENO, readTerminal, sizeof(readTerminal));

			// Check command
			readTerminal[strlen(readTerminal)-1] = '\0';
			if (strcmp(readTerminal, "/quit") == 0){
				pthread_mutex_lock(&EDIT_MODE);
				mode = QUIT;
				pthread_mutex_unlock(&EDIT_MODE);
				return NULL;
			}
			else if(readTerminal[0] == '/'){
				char sendbuffer[1500];
				memset(sendbuffer, 0, sizeof(sendbuffer));
				sprintf(sendbuffer, "Command %s not recognized\n", readTerminal);
				write(STDOUT_FILENO, sendbuffer, strlen(sendbuffer));
			}

			// Print prompt for newline
			if (checkMode == WAIT) write(STDOUT_FILENO, promptID, strlen(promptID));
		}
	}

	printf("FAILED TO END WRITE THREAD CLEANLY\n");
	return NULL;
}

void* waitRead(void* v){
	int* fd = (int*) v;
	// Setup Socket
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0){
		fprintf(stderr, "Error: Socket Creation failed\n");
		return NULL;
	}
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	
	// Set up address
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = 0;
	if (bind(listen_fd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
		fprintf(stderr, "Error: Bind Failed\n");
		close (listen_fd);
		return NULL;
	}
	

	// Get Port
	struct sockaddr addr;
	socklen_t addrlen = sizeof(addr);
	int sockname = getsockname(listen_fd, &addr, &addrlen);
	if (sockname < 0){
		fprintf(stderr, "Error: Failed to get socket details\n");
		close (listen_fd);
		close(*fd);
		return NULL;
	}
	struct sockaddr_in* ipv4Addr = (struct sockaddr_in*) &addr;
	char buffer[1024];
	memset(buffer, 0 , sizeof(buffer));
	sprintf(buffer, "Waiting\n\n%d", ipv4Addr->sin_port);

	// Send Server waiting status
	if (send(*fd, buffer, strlen(buffer), 0) <= 0) {
		fprintf(stderr, "ERROR: Send to host server failed\n");
		close (listen_fd);
		close (*fd);
		return NULL;
	}

	// Listen for clients
	listen(listen_fd, 500);
	int comm_fd = -1;

	int res = 0;
	while(res >= 0){
		// Check for END of WAIT
		pthread_mutex_lock(&EDIT_MODE);
		int checkMode = mode;
		pthread_mutex_unlock(&EDIT_MODE);
		if (checkMode != WAIT) return NULL;

		// Check for connections
		res = input_timeout(listen_fd, 0);
		if (res > 0){
			comm_fd = accept(listen_fd, (struct sockaddr*) NULL, NULL);
			if (comm_fd != -1) break;
		}

	}

	// Check mode
	pthread_mutex_lock(&EDIT_MODE);
	mode = WAIT_SUC;
	pthread_mutex_unlock(&EDIT_MODE);

	// Return socket descriptor
	int* rere = &comm_fd;
	return rere;
}



// Multithreading functions
int handleChat(struct Client cData){
	// Set Status
	pthread_mutex_lock(&EDIT_MODE);
	mode = CHAT;
	pthread_mutex_unlock(&EDIT_MODE);

	struct Client pass;
	pass.fd = cData.fd;

	// Get / Send ID if needed
	char temp[1024];
	memset(temp, 0 ,sizeof(temp));
	if (strcmp(cData.ID, "GETNAME\n") == 0){
		int r = 0;
		while(r == 0){
			r = recv(cData.fd, temp, sizeof(temp), 0);
			if (r < 0) {
				fprintf(stderr, "ERROR: recv to client get Name\n");
				close (cData.fd);
				return -1;
			}
		}
		strcpy(pass.ID, temp);
	}
	else{
		if (send(cData.fd, GlobalID, strlen(GlobalID), 0) < 0) {
			fprintf(stderr, "ERROR: send to client get Name\n");
			close (cData.fd);
			return -1;
		}
		strcpy(pass.ID, cData.ID);
	}

	// Connected
	memset(temp, 0 ,sizeof(temp));
	sprintf(temp, "Connected to %s\n", pass.ID);
	write(STDOUT_FILENO, temp, strlen(temp));


	// Create threads to read and write
	pthread_t writeProcess;
	if (pthread_create(&writeProcess, NULL, chatWrite, &pass)){
		fprintf(stderr, "Error: Failed to crate a new thread\n");
		close (pass.fd);
		return -1;

	}
	pthread_t readProcess;
	if (pthread_create(&readProcess, NULL, chatRead, &pass)){
		fprintf(stderr, "Error: Failed to crate a new thread\n");
		close (pass.fd);
		return -1;
	}

	// Join both threads
	if(pthread_join(writeProcess, NULL) != 0){
		fprintf(stderr, "Error: Failed to join thread\n");
		close (pass.fd);
		return -1;
	}
	if(pthread_join(readProcess, NULL) != 0){
		fprintf(stderr, "Error: Failed to join thread\n");
		close (pass.fd);
		return -1;
	}

	memset(temp, 0, sizeof(0));
	sprintf(temp, "Left conversation with %s.\n", pass.ID);
	write(STDOUT_FILENO, temp, strlen(temp));

	return 1;
}

int handleWait(int fd){
	// Initiate Wait
	pthread_mutex_lock(&EDIT_MODE);
	mode = WAIT;
	pthread_mutex_unlock(&EDIT_MODE);

	write(STDOUT_FILENO, "Waiting for connection.\n", strlen("Waiting for connection.\n"));

	// Read Terminal
	pthread_t writeProcess;
	if (pthread_create(&writeProcess, NULL, waitWrite, NULL)){
		fprintf(stderr, "Error: Failed to crate a new thread\n");
		close (fd);
		return -1;
	}
	
	// Wait for server to send client to chat
	pthread_t readProcess;
	if (pthread_create(&readProcess, NULL, waitRead, &fd)){
		fprintf(stderr, "Error: Failed to crate a new thread\n");
		close (fd);
		return -1;
	}
	
	// Join both threads
	if(pthread_join(writeProcess, NULL) != 0){
		fprintf(stderr, "Error: Failed to join thread\n");
		close (fd);
		return -1;
	}
	
	int* new_FD = NULL;
	if(pthread_join(readProcess, (void**)&new_FD) != 0){
		fprintf(stderr, "Error: Failed to join thread\n");
		close (fd);
		return -1;
	}
	
	// End waiting
	if (send(fd, "END_Wait\n\n", strlen("END_Wait\n\n"), 0) < 0) {
		fprintf(stderr, "ERROR: send to host server failed\n");
		close (fd);
		return -1;
	}

	// Start Chat if called
	if (new_FD != NULL){
		struct Client cData;
		cData.fd = *new_FD;
		strcpy(cData.ID, "GETNAME\n");

		if (mode == WAIT_SUC){
			return handleChat(cData);
		}
		else printf("IP can't be found \n");

		close(*new_FD);
	}
	else if (mode == WAIT_END || mode == QUIT) printf("Stopped waiting\n");

	return 1;
}

int handleConnect(int fd, char* ID){
	char buffer[1024];
	memset(buffer, 0 ,sizeof(buffer));
	sprintf(buffer, "ConnectTo\n\n%s", ID);

	// Send Request For Client Data
	if (send(fd, buffer, strlen(buffer), 0) < 0) {
		fprintf(stderr, "ERROR: send to host server failed\n");
		close (fd);
		return -1;
	}

	// Get Data
	struct SendData sd;
	if (recv(fd, &sd, sizeof(sd), 0) < 0) {
		fprintf(stderr, "ERROR: send to host server failed\n");
		close (fd);
		return -1;
	}
	// Check if valid
	if (sd.port <= 0){
		memset(buffer, 0 ,sizeof(buffer));
		sprintf(buffer, "%s is not availiable\n", ID);
		write(STDOUT_FILENO, buffer, strlen(buffer));
		return 0;
	}
	else{
		struct sockaddr_in servaddr;
		memset(&servaddr, 0, sizeof servaddr);
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = sd.port;

		int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
		if(listen_fd < 0){
			fprintf(stderr, "ERROR: Socket could not be created\n");
			return -1;
		}

		int inetCheck = inet_pton(AF_INET, sd.IP, &servaddr.sin_addr);
		if (inetCheck <= 0){
			fprintf(stderr, "ERROR: Invalid address\n");
			close (listen_fd);
			return -1;
		}

		
		// Connect to address
		if (connect(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
			fprintf(stderr, "ERROR: Connection Failed\n");
			fprintf(stderr, "\t%s\n", strerror(errno));
			close (listen_fd);
			return -1;
		}
		struct Client c;
		c.fd = listen_fd;
		strcpy(c.ID, ID);
		return handleChat(c);
	}
	return -1;
}

// Handle Functions
// 1 = success 0 = error
int callCommand(char* command, int fd){
	// Gets List of connections
	if (strcmp(command, "/list") == 0){
		if (send(fd, "GetList\n\n", strlen("GetList\n\n"), 0) <= 0) {
			fprintf(stderr, "ERROR: Send to host server failed\n");
			close (fd);
			return -1;
		}

		char buffer[1024];
		memset(buffer, 0 , sizeof(buffer));
		if (recv(fd, buffer, sizeof(buffer), 0) < 0) {
			fprintf(stderr, "ERROR: recv from host server failed\n");
			close (fd);
			return -1;
		}
		write(STDOUT_FILENO, buffer, strlen(buffer));
		return 1;
	}

	// Wait for connection
	else if (strcmp(command, "/wait") == 0){
		return handleWait(fd);
	}

	// Connect with ID
	else if (strstr(command, "/connect ") != NULL){
		char* ID = command + 9;
		handleConnect(fd, ID);
		return 1;
	}

	// Ends Client
	else if (strcmp(command, "/quit") == 0){
		pthread_mutex_lock(&EDIT_MODE);
		mode = QUIT;
		pthread_mutex_unlock(&EDIT_MODE);
		return 1;
	}

	// Unimplemented Command
	else{
		char sendback[1024];
		sprintf(sendback, "Command %s not recognized\n", command);
		int w = write(STDOUT_FILENO, sendback, strlen(sendback));
		if (w < 0){
			fprintf(stderr, "ERROR: Failed to write to stdout\n");
			return 0;
		}
		return 1;
	}
	return 0;
}

// Handle Signal interrupt
void sigintHandler(int sig_num) { 
	pthread_mutex_lock(&EDIT_MODE);
	if (mode == CHAT || mode == WAIT) {
		--mode;
	}
	pthread_mutex_unlock(&EDIT_MODE);	

	write(STDIN_FILENO, "\n", strlen("\n"));
	signal(SIGINT, sigintHandler); 
}


//Main ------------------------------------------------------------------------
int main(int argc, char **argv){
	// Initiate global values
	mode = INFO;
	if (pthread_mutex_init(&EDIT_MODE, NULL) != 0){
		fprintf(stderr, "Error: Failed to Init mutex");
		exit(1);
	}

	if (argc == 3){
		fprintf(stderr, "Usage: myclient <IP> <Port> <ID>\n");
		exit(1);
	}
	char* ip = argv[1];
	char* portString = argv[2];
	strcpy(GlobalID, argv[3]);
	strcpy(promptID, argv[3]);
	strcat(promptID, "> ");

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

	// Get IP
	struct ifaddrs *ifap;
	struct ifaddrs *ifa;
	struct sockaddr_in *sa;
	char *addrSTR;

	if(getifaddrs (&ifap) == -1){
		fprintf(stderr, "ERROR: Failed to get IP address\n");
		close (listen_fd);
		exit(1);
	}
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr && ifa->ifa_addr->sa_family==AF_INET) {
			sa = (struct sockaddr_in *) ifa->ifa_addr;
			addrSTR = inet_ntoa(sa->sin_addr);
			if(strcmp(addrSTR, "127.0.0.1") != 0) break;
		}
	}

	freeifaddrs(ifap);
	//printf("Interface: %s\tAddress: %s\n", ifa->ifa_name, addrSTR);


	// Send Data
	char buffer[1024];
	memset(buffer, 0, sizeof(buffer));
	sprintf(buffer, "%s\n%s", argv[3], addrSTR);

	if (send(listen_fd, buffer, strlen(buffer), 0) <= 0) {
		fprintf(stderr, "ERROR: Send to host server failed\n");
		close (listen_fd);
		exit(1);
	}

	// Recv Confirmation
	memset(buffer, 0 , sizeof(buffer));
	if (recv(listen_fd, buffer, sizeof(buffer), 0) <= 0) {
		fprintf(stderr, "ERROR: Recv ID Confirm from Client Failed\n");
		close (listen_fd);
		exit(1);
	}

	// Determine if ID was declined
	if (strcmp(buffer, "Bad ID\n\n") == 0){
		fprintf(stderr, "Client ID is already taken\n");
		close (listen_fd);
		exit(1);
	}

	// Override SIGINT
	signal(SIGINT, sigintHandler);

	// INFO - Read from terminal
	char readTerminal[1024];
	while(1){
		// End if Quit has been resolved
		if (mode == QUIT) break;
		if (mode == WAIT_END || mode == CHAT_END) mode = INFO;

		// Write ID and read input
		write(STDOUT_FILENO, promptID, strlen(promptID));
		memset(readTerminal, 0, sizeof(readTerminal));
		read(STDIN_FILENO, readTerminal, sizeof(readTerminal));
		readTerminal[strlen(readTerminal)-1] = '\0';

		// Determine Command
		if (readTerminal[0] == '/'){
			if (callCommand(readTerminal, listen_fd) == 0) break;
		}

		
	}

	sleep(1);

	// End Connection
	if (send(listen_fd, "Terminate\n\n", strlen("Terminate\n\n"), 0) <= 0) {
		fprintf(stderr, "ERROR: Send to host server failed\n");
		close (listen_fd);
		exit(1);
	}

  
	
	close (listen_fd);
	pthread_mutex_destroy (&EDIT_MODE);
}
