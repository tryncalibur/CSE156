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
pthread_mutex_t WRITE_ENTRY;
pthread_mutex_t SPRINTF_USE;

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
int input_timeout (fd_set set, unsigned int secs){
	struct timeval timeout;
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
	while(fgets(line, sizeof(line), forbiddenSites) != NULL){
		if (strstr(line, " ") != NULL){
			fprintf(stderr, "Error: File does not follow correct format\n");
			fclose(forbiddenSites);
	    	return -1;
		}

		line[strlen(line)-2] = '\0';
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
	if (temp == NULL){
		free (rere);
		pthread_mutex_unlock(&GET_HOST);
    	return NULL;
	}
	*rere = *temp;
	pthread_mutex_unlock(&GET_HOST);
	return rere;
}

// Log client entry with time
int logEntry(char* clientIP, char* request, char* host, int code, int size){
	char rere[1024];
	memset(rere, 0, sizeof(rere));

	time_t now = time(NULL);
	struct tm format = *localtime(&now);

	pthread_mutex_lock(&SPRINTF_USE);
	// Get Time
	char currentTime[50];
	memset(currentTime, 0, sizeof(currentTime));
	sprintf(currentTime, "%d-%02d-%02dT%02d:%02d:%02dU", 
		format.tm_year + 1900, format.tm_mon + 1, format.tm_mday, format.tm_hour, format.tm_min, format.tm_sec);

	// Make String
	char temp[1024];
	sprintf(temp, "%s %s \"%s\" %s %d %d\n", 
		currentTime, clientIP, request, host, code, size);
	strcpy(rere, temp);
	pthread_mutex_unlock(&SPRINTF_USE);

	// Write To file
	pthread_mutex_lock(&WRITE_ENTRY);
	FILE* f = fopen("access.log", "a");
	if (f == NULL){
		fprintf(stderr, "Error: Unable to read / write file\n");
		pthread_mutex_unlock(&WRITE_ENTRY);
		return -1;
	}
	fwrite(rere, sizeof(char), strlen(rere), f);
	fclose(f);
	pthread_mutex_unlock(&WRITE_ENTRY);

	return 1;
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
	if (inByte <= 0){
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


	// Remove connection header
	char* bad = strstr(recvbuffer, "Proxy-Connection: ");
	if (bad != NULL){
		char* endbad = strstr(bad, "\r\n");
		endbad+= 2;
		char str1[1024];
		memset(str1, 0, sizeof(str1));
		strncpy(str1, recvbuffer, bad - &recvbuffer[0]);
		strcat(str1, endbad);
		memset(recvbuffer, '\0', sizeof(recvbuffer));
		strcpy(recvbuffer, str1);
	}
	bad = strstr(recvbuffer, "Connection: ");
	if (bad != NULL){
		char* endbad = strstr(bad, "\r\n");
		endbad+= 2;
		char str1[1024];
		memset(str1, 0, sizeof(str1));
		strncpy(str1, recvbuffer, bad - &recvbuffer[0]);
		strcat(str1, endbad);
		memset(recvbuffer, '\0', sizeof(recvbuffer));
		strcpy(recvbuffer, str1);
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
		bytesRead = contentLength;
		
		// Countdown from content length from current packet
		char* startcount = strstr(recvbuffer, "\r\n\r\n");
		if (startcount == NULL){
			fprintf(stderr, "ERROR: Expected \\r\\n\\r\\n in header\n");
			return rere;
		}
		startcount += 4;

		if (contentLength > strlen(startcount) || contentLength != 0){
			if (send(client_fd, recvbuffer, inByte, 0) <= 0) {
				fprintf(stderr, "ERROR: Send to client failed\n");
				return rere;
			}

			contentLength -= strlen(startcount);
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
			memset(recvbuffer, '\0', sizeof(recvbuffer));
			inByte = recv(server_fd, recvbuffer, sizeof(recvbuffer), 0);
			if (inByte <= 0){
				fprintf(stderr, "ERROR: Recieve failed\n");
				return rere;
			}

			if (contentLength > inByte){
				contentLength -= inByte;
				if (send(client_fd, recvbuffer, inByte, 0) <= 0) {
					fprintf(stderr, "ERROR: Send to client failed\n");
					return rere;
				}

			}
			else{
				if (send(client_fd, recvbuffer, inByte, 0) <= 0) {
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
		//get Chunk
		char* getChunk = strstr(recvbuffer, "\r\n\r\n") + 4;
		char* endl = NULL;
		int currentChunk = strtol(getChunk, &endl, 16);
		bytesRead = currentChunk;
		
		// Send First
		recvbuffer[inByte] = '\0';
		if (send(client_fd, recvbuffer, inByte, 0) <= 0) {
			fprintf(stderr, "ERROR: Send to client failed\n");
			return rere;
		}
		currentChunk -= inByte;

		// Read in chunks
		while(1){
			memset(recvbuffer, 0, sizeof(recvbuffer));
			inByte = recv(server_fd, recvbuffer, sizeof(recvbuffer), 0);
			if (inByte <= 0){
				fprintf(stderr, "ERROR: Recieve failed\n");
				return rere;
			}

			if (strstr(recvbuffer, "0\r\n\r\n") != NULL){
				if (send(client_fd, recvbuffer, strlen(recvbuffer), 0) <= 0) {
					fprintf(stderr, "ERROR: Send to client failed\n");
					return rere;
				}
				rere.code = resCode;
				rere.bytes = bytesRead;
				return rere;
			}

			// Check if new chunk
			if (currentChunk <= 0){
				currentChunk = strtol(recvbuffer, &endl, 16);
				bytesRead += currentChunk;
			}

			// Send chunk
			recvbuffer[inByte] = '\0';
			if (send(client_fd, recvbuffer, strlen(recvbuffer), 0) <= 0) {
				fprintf(stderr, "ERROR: Send to client failed\n");
				return rere;
			}
			currentChunk -= inByte;
		}		
	}

	fprintf(stderr, "ERROR: Only support chunked encoding or content length\n");
	return rere;
}

struct ServerRes handleConnect(int client_fd, int server_fd, char* clientbuffer){
	struct ServerRes rere;
	rere.bytes = -1;
	rere.code = -1;
	if (send(server_fd, clientbuffer, strlen(clientbuffer), 0) <= 0) {
		fprintf(stderr, "ERROR: Send to host server failed\n");
	}
	printf ("%s\n\n\n", clientbuffer);
	
	char recvbuffer[1024];
	while(1){
		memset(recvbuffer, 0, sizeof(recvbuffer));
		int inByte = recv(server_fd, recvbuffer, sizeof(recvbuffer), 0);
		if (inByte <= 0){
			fprintf(stderr, "ERROR: Recieve failed\n");	
			break;	
			}
		printf("---------\n%s\n\n\n",recvbuffer);
	}
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
	if (recvReq <= 0){
		fprintf(stderr, "Error: Recv Failed from client\n");
		close(sock);
		return NULL;
	}

	// Set to -1 if request format is wrong
	// Set to -2 if unimplemented command
	// Set to -3 if forbidden site
	int status = 0;

	// Get RequestLine
	char requestLine[1024];
	memset(requestLine, 0, sizeof(requestLine));
	char* endline = strchr(buffer, '\r');
	strncpy(requestLine, buffer, endline-buffer);

	// Get Host
	char hostAr[256];
	memset(hostAr, 0, sizeof(hostAr));
	char* host = strstr(buffer, "Host: ");
	if (host == NULL) {
		strcpy(hostAr, "");
		status = -1;
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

	// Remove Connection Header
	char* bad = strstr(buffer, "Proxy-Connection: ");
	if (bad != NULL){
		char* endbad = strstr(bad, "\r\n");
		endbad+= 2;
		char str1[1024];
		memset(str1, 0, sizeof(str1));
		strncpy(str1, buffer, bad - &buffer[0]);
		strcat(str1, endbad);
		memset(buffer, 0, sizeof(buffer));
		strcpy(buffer, str1);
	}
	bad = strstr(buffer, "Connection: ");
	if (bad != NULL){
		char* endbad = strstr(bad, "\r\n");
		endbad+= 2;
		char str1[1024];
		memset(str1, 0, sizeof(str1));
		strncpy(str1, buffer, bad - &buffer[0]);
		strcat(str1, endbad);
		memset(buffer, 0, sizeof(buffer));
		strcpy(buffer, str1);
	}

	//-----------------------------------------------------------------------------
	// Get Host
	struct hostent* hostData= gethostbyname_ts(hostAr);
	if(hostData == NULL){
		fprintf(stderr, "Error: gethostbyname failed to return successfully\n");
		status = -1;
	}

	// Do Request if valid
	if (status == 0){
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

		// Do something First time
		if (memcmp("GET ", buffer, 4) == 0){
			struct ServerRes get = handleGet(sock, host_fd, buffer);
			if (get.code == -1 && get.bytes == -1){
				close(sock);
				close(host_fd);
				return NULL;
			}

			// Log Entry
			logEntry(tdata->cIP, requestLine, hostAr, get.code, get.bytes);
		}
		else if (memcmp("CONNECT ", buffer, 8) == 0){
			struct ServerRes con = handleConnect(sock, host_fd, buffer); 
			if (con.code == -1 && con.bytes == -1){
				close(sock);
				close(host_fd);
				return NULL;
			}

			// Log Entry
			logEntry(tdata->cIP, requestLine, hostAr, con.code, con.bytes);
		}
		// Unimplemented Method
		else{
			char* methodRejected = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
            if (send(sock, methodRejected, strlen(methodRejected), 0) <= 0) {
				fprintf(stderr, "ERROR: Send to host server failed\n");
				close(sock);
				close(host_fd);
				return NULL;
			}

			// Log Entry
			logEntry(tdata->cIP, requestLine, hostAr, 405, 0);
		}


		// Maintain Connections
		int res = 0;
		fd_set list;
		FD_ZERO(&list);
		FD_SET(sock, &list);
		FD_SET(host_fd, &list);

		// Check for Sent data
    	while(res >=0){
    		res = input_timeout(list, 300);
    		if (res > 0){
    			if (FD_ISSET(sock, &list)){
    				memset (buffer, 0, sizeof(buffer));
				 	recvReq = recv(sock, buffer, sizeof(buffer), 0);
					if (recvReq <= 0){
						if (recvReq < 0) fprintf(stderr, "Error: Send Failed from client x2\n");
						close(host_fd);
						close(sock);
						return NULL;
					}
					// Get RequestLine
					memset(requestLine, 0, sizeof(requestLine));
					endline = strchr(buffer, '\r');
					strncpy(requestLine, buffer, endline-buffer);

					// DO STUFF Repeat
					// Check Request CONN
					if (memcmp("GET ", buffer, 4) == 0){
						struct ServerRes get = handleGet(sock, host_fd, buffer);
						if (get.code == -1 && get.bytes == -1){
							close(sock);
							close(host_fd);
							return NULL;
						}

						// Log Entry
						logEntry(tdata->cIP, requestLine, hostAr, get.code, get.bytes);
					}
					else if (memcmp("CONNECT ", buffer, 8) == 0){
						struct ServerRes con = handleConnect(sock, host_fd, buffer); 
						if (con.code == -1 && con.bytes == -1){
							close(sock);
							close(host_fd);
							return NULL;
						}

						// Log Entry
						logEntry(tdata->cIP, requestLine, hostAr, con.code, con.bytes);
					}
					// Unimplemented Method
					else{
						char* methodRejected = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
			            if (send(sock, methodRejected, strlen(methodRejected), 0) <= 0) {
							fprintf(stderr, "ERROR: Send to host server failed\n");
							close(sock);
							close(host_fd);
							return NULL;
						}

						// Log Entry
						logEntry(tdata->cIP, requestLine, hostAr, 405, 0);
					} 
					
    			}
    			else{
    				memset (buffer, 0, sizeof(buffer));
				 	recvReq = recv(host_fd, buffer, sizeof(buffer), 0);

				 	// Server initiated Shutdown
				 	if (recvReq <= 0){
						close(host_fd);
						close(sock);
						return NULL;
					}
    			}
    		}
    		// Timeout
    		else{
				fprintf(stderr, "Error: Client / Server Timout\n");
				close(host_fd);
				close(sock);
				return NULL;
    		}
    	}
		

		close(host_fd);
	}

	// Send error response to client
	else {
		char* methodRejected = NULL;
		int badCode = 0;
		if (status == -1){
			methodRejected = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
			badCode = 400;
		}
		if (status == -3){
			methodRejected = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
			badCode = 403;
		}
		else {
			fprintf(stderr, "ERROR: Unknown status: %d\n", status);
			close(sock);
			return NULL;
		}

		// Send back to client
        if (send(sock, methodRejected, strlen(methodRejected), 0) <= 0) {
			fprintf(stderr, "ERROR: Send to client failed\n");
			close(sock);
			return NULL;
		}

		// Log Entry
		logEntry(tdata->cIP, requestLine, hostAr, badCode, 0);
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
	if (pthread_mutex_init(&WRITE_ENTRY, NULL) != 0){
		fprintf(stderr, "Error: Failed to Init mutex");
    	exit(1);
	}
	if (pthread_mutex_init(&SPRINTF_USE, NULL) != 0){
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
		//close(comm_fd);
	}

	close(listen_fd);
	pthread_mutex_destroy (&GET_HOST);
	pthread_mutex_destroy (&WRITE_ENTRY);
	pthread_mutex_destroy (&SPRINTF_USE);
}
