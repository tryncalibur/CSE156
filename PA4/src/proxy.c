#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <pthread.h>
#include <netdb.h> 

// Global Scope
pthread_mutex_t GET_HOST;

// Tread Data Structure
struct ThreadData{
    int sockfd;
    char fsite[256];
    char cIP[INET_ADDRSTRLEN];
};

struct ServerRes{
	int bytes;
	int code;
};

// Helper Functions---------------------------------------------------------------------------------
// Use to periodically check on socket
int input_timeout (int fd, unsigned int secs){
	fd_set set;
	struct timeval timeout;

	FD_ZERO (&set);
	FD_SET (fd, &set);

	timeout.tv_sec = secs;
	timeout.tv_usec = 0;
	return(select(FD_SETSIZE, &set, NULL, NULL, &timeout));
}

// Check if file is authorized
// -1 = FILE error
//  0 = shares name
//  1 = not shared
int checkSites(char* fsite_name, char* siteName){
	FILE* forbiddenSites = fopen(fsite_name, "rb");
	if (forbiddenSites == NULL){
		fprintf(stderr, "Error: Unable to Open <forbidden-sites-file>\n");
    	return -1;
	}

	// Compare sites
	char line[1024];
	char* rere;
	while(fgets(line, sizeof(line), forbiddenSites)){
		rere = strstr(siteName, line);
		if (rere != NULL) break;
	}
	fclose(forbiddenSites);
	if (rere == NULL) return 1;
	else return 0;
}

// Thread safe version of gethostbyname. 
// Required to free data after called
struct hostent* gethostbyname_ts(char* name){
	struct hostent* rere = malloc(sizeof(struct hostent));
	if (rere == NULL){
		fprintf(stderr, "Error: Failed to malloc\n");
		return NULL;
	}
	pthread_mutex_lock(&GET_HOST);
	struct hostent* temp = gethostbyname(name);
	*rere = *temp;
	pthread_mutex_unlock(&GET_HOST);
	return rere;
}

//------------------------------------------------------------------------------------------------------
// Called to handle Get Requests
struct ServerRes handleGet(int client_fd, int server_fd, char* clientbuffer){
	// Send Get Request to Server
	struct ServerRes rere;
	rere.bytes = -1;
	rere.code = -1;
	if (send(server_fd, clientbuffer, strlen(clientbuffer), 0) <= 0) {
		fprintf(stderr, "ERROR: Send to host server failed\n");
		return rere;
	}

	int bytesRead = 0;
	char recvbuffer[1024];
	memset(recvbuffer, '\0' , sizeof(recvbuffer));
	int inByte = recv(server_fd, recvbuffer, sizeof(recvbuffer), 0);
	if (inByte < 0){
		fprintf(stderr, "ERROR: Recieve failed\n");
		return rere;
	}

	// Get Response Code
	int resCode = 0;
	char code_str[5];
	strncpy(code_str, recvbuffer + 9, 3);
	for (int i = 0; code_str[i] != '\0'; ++i){
    	resCode = resCode * 10 + code_str[i] - '0';
	}

	// Determine encoding
	// Deal with content length
	char* getTypeCL = strstr(recvbuffer, "Content-Length: ");
	if (getTypeCL != NULL){
		// Get Content Length as string
		getTypeCL += 16;
		char* endofline = strstr(getTypeCL, "\r\n");
		char size_str[10];
		strncpy(size_str, getTypeCL, endofline-getTypeCL);
		
		// Get Content Length as int
		int contentLength = 0;
		for (int i = 0; size_str[i] != '\0'; ++i){
        	contentLength = contentLength * 10 + size_str[i] - '0';
		}
		
		// Countdown from content length from current packet
		char* startcount = strstr(recvbuffer, "\r\n\r\n");
		if (startcount == NULL){
			fprintf(stderr, "ERROR: Expected \\r\\n\\r\\n in header\n");
			return rere;
		}
		startcount += 4;

		if (contentLength > strlen(startcount) || contentLength != 0){
			if (send(client_fd, recvbuffer, strlen(recvbuffer)-1, 0) <= 0) {
				fprintf(stderr, "ERROR: Send to client failed\n");
				return rere;
			}

			contentLength -= strlen(startcount);
			bytesRead += inByte;
		}
		else{
			char temp[1024];
			memset(temp, 0, sizeof(temp));
			strncpy(temp, recvbuffer, &recvbuffer[0] - startcount + contentLength);
			if (send(client_fd, temp, strlen(temp), 0) <= 0) {
				fprintf(stderr, "ERROR: Send to client failed\n");
				return rere;
			}

			rere.bytes = strlen(temp);
			rere.code = resCode;
			return rere;
		}

		// Get rest of packets
		while(1){
			memset(recvbuffer, 0, sizeof(recvbuffer));
			inByte = recv(server_fd, recvbuffer, sizeof(recvbuffer), 0);
			if (inByte < 0){
				fprintf(stderr, "ERROR: Recieve failed\n");
				return rere;
			}

			if (contentLength > inByte){
				bytesRead += inByte;
				contentLength -= inByte;
				if (send(client_fd, recvbuffer, strlen(recvbuffer), 0) <= 0) {
					fprintf(stderr, "ERROR: Send to client failed\n");
					return rere;
				}

			}
			else{
				bytesRead += contentLength;
				if (send(client_fd, recvbuffer, strlen(recvbuffer), 0) <= 0) {
					fprintf(stderr, "ERROR: Send to client failed\n");
					return rere;
				}
				rere.bytes = bytesRead;
				rere.code = resCode;
				return rere;
			}

		}
	}
	// Deal with chunked encoding
	getTypeCL = strstr(recvbuffer, "Transfer-Encoding: chunked");
	if (getTypeCL != NULL){
		


	}

	fprintf(stderr, "ERROR: Only support chunked encoding or content length\n");
	return rere;
}


// Multithread function---------------------------------------------------------------------------------
// Handle Client's request
void* handleRequest(void* fd){
	char buffer[1024];
	memset(buffer, 0, sizeof(buffer));
	struct ThreadData* tdata = (struct ThreadData*)fd;
	int sock = tdata->sockfd;

	// Recieve Data from client
	int recvReq = recv(sock, buffer, sizeof(buffer), 0);
	if (recvReq < 0){
		fprintf(stderr, "Error: Recv Failed from client\n");
		close(sock);
		return NULL;
	}

	// Get Time
	char currentTime[1024];
	time_t now = time(NULL);
	struct tm format = *localtime(&now);
	sprintf(currentTime, "%d-%02d-%02dT%02d:%02d:%02d", 
		format.tm_year + 1900, format.tm_mon + 1, format.tm_mday, format.tm_hour, format.tm_min, format.tm_sec);

	// Set to -1 if request format is wrong
	// Set to -2 if unimplemented command
	// Set to -3 if forbidden site
	char status = 0;

	// Get RequestLine
	char requestLine[1024];
	char* endline = strchr(buffer, '\r');
	strncpy(requestLine, buffer, endline-buffer);

	// Get Host
	char hostAr[256];
	memset(hostAr, 0, sizeof(hostAr));
	char* host;
	host = strstr(buffer, "Host: ");
	if (host == NULL) {
		strcpy(hostAr, "");
		status =  -1;
	}
	else {
		host += 6;
		char* end = strchr(host, '\r');
		strncpy(hostAr, host, end-host);
	}

	// Get Port
	int port = 80;
	char* check = strchr(hostAr, ':');
	if (host != NULL && check != NULL){
		hostAr[strlen(hostAr) - strlen(check)] = '\0';
		++check;
		port = 0;
		for (int i = 0; check[i] != '\0'; ++i){
        	port = port * 10 + check[i] - '0';
		}
	}
	
	// Check forbidden site list
	int siteAllowed = checkSites(tdata->fsite, host);
	if (siteAllowed == -1){
		fprintf(stderr, "Error: Failed to open file\n");
		close(sock);
		return NULL;
	}
	else if (siteAllowed == 0) if (status == 0) status = -3;

	//-----------------------------------------------------------------------------

	// Do Request if valid
	if ((memcmp("GET ", buffer, 4) == 0 || memcmp("CONNECT ", buffer, 8) == 0) && status == 0){
		struct hostent* hostData= gethostbyname_ts(hostAr);
		if(hostData == NULL){
			fprintf(stderr, "Error: gethostbyname failed to return successfully\n");
			close(sock);
			return NULL;
		}

		// Get Server IP
		char serverIP[256];
		struct in_addr **addr_list = (struct in_addr **) hostData->h_addr_list;
		strcpy(serverIP , inet_ntoa(*addr_list[0]));
		free(hostData);

		// Setup Socket connections
		struct sockaddr_in hostserv;
		memset(&hostserv, 0, sizeof hostserv);
		hostserv.sin_family = AF_INET;
		hostserv.sin_port = htons(port);

		int host_fd = socket(AF_INET, SOCK_STREAM, 0);
		if(host_fd < 0){
			fprintf(stderr, "ERROR: Socket could not be created\n");
			close(sock);
			return NULL;
		}

		int inetCheck = inet_pton(AF_INET, serverIP, &hostserv.sin_addr);
			if (inetCheck <= 0){
			fprintf(stderr, "ERROR: Invalid address\n");
			close(sock);
			close(host_fd);
			return NULL;
		}

		if (connect(host_fd, (struct sockaddr*)&hostserv, sizeof(hostserv)) < 0){
			fprintf(stderr, "ERROR: Connection Failed\n");
			fprintf(stderr, "\t%s\n", strerror(errno));
			close(sock);
			close(host_fd);
			return NULL;
		}


		// Do something
		if (memcmp("GET ", buffer, 4) == 0){
			struct ServerRes get = handleGet(sock, host_fd, buffer);
			if (get.code == -1 && get.bytes == -1){
				close(sock);
				close(host_fd);
				return NULL;
			}
		}
		else if (memcmp("CONNECT ", buffer, 8) == 0){

		}

		close(host_fd);
	}

	// Send error response to client
	else {
		if(status == 0) status = -2;

	}


	close(sock);
	return NULL;
}





// Main Server function ----------------------------------------------------------------------------------
int main(int argc, char **argv){
	if (pthread_mutex_init(&GET_HOST, NULL) != 0){
		fprintf(stderr, "Error: Failed to Init mutex");
    	exit(1);
	}

	// Check Arguements
	if (argc != 3){
		fprintf(stderr, "Expected Format: ./proxy <port-number> <forbidden-sites-file>\n");
    	exit(1);
	}

	// Check if valid Port
	int port = atoi(argv[1]);
	if (port == 0){
		fprintf(stderr, "Error: Enter valid port");
    	exit(1);
	}

	// Check for forbidden Sites in file
	if (access (argv[2], F_OK | R_OK) != 0){
		fprintf(stderr, "ERROR: File %s doesn't exist\n", argv[2]);
		exit(1);
	}

	//Create sockets and Connections
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

	//-----------------------------------------------------------------------------------------------

	// Listen for clients
	int comm_fd;
	struct sockaddr client;
	socklen_t len;
	while(1){
		comm_fd = accept(listen_fd, &client, &len);
		if (comm_fd < 0){
			fprintf(stderr, "Error: Accept Failed\n");
			close (listen_fd);
			exit(1);
		}
		// Connected with Client
		else{
			// Create Thread Data
			struct ThreadData td;
			memset(&td, 0, sizeof(td));
			td.sockfd = comm_fd;
			strcpy(td.fsite, argv[2]);

			// Get Client IP  
			struct sockaddr addr;
			socklen_t addrlen;
			int sockname = getsockname(comm_fd, &addr, &addrlen);
			if (sockname < 0){
				fprintf(stderr, "Error: Failed to get socket details\n");
				close (listen_fd);
				close(comm_fd);
				exit(1);
			}
			char clientIP[INET_ADDRSTRLEN];
			struct sockaddr_in* ipv4Addr = (struct sockaddr_in*) &addr;
			inet_ntop(AF_INET, &(ipv4Addr->sin_addr), clientIP, INET_ADDRSTRLEN);
			strcpy(td.cIP, clientIP);


			// Handle Request in new thread
			pthread_t newProcess;
			if (pthread_create(&newProcess, NULL, handleRequest, &td)){
				fprintf(stderr, "Error: Failed to crate a new thread\n");
				close (listen_fd);
				close(comm_fd);
				exit(1);
			}
			sleep(1);
		}
		close(comm_fd);
	}

	close(listen_fd);
	pthread_mutex_destroy (&GET_HOST);
}
