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
#include <ctype.h>

// Structs
struct Node{ 
    char ID [256];
    char IP [INET_ADDRSTRLEN];
    int port;
    int status;
    struct Node* next; 
}; 

struct SendData{
	char ID [256];
    char IP [INET_ADDRSTRLEN];
    int port;
};

struct ThreadData{
	struct Node* ClientData;
	int client_fd;
};

// GLOBAL SCOPE
struct Node* LIST;
pthread_mutex_t EDIT_LIST;

// Utility ----------------------------------------------------------------------------
int input_timeout (int fd, unsigned int secs){
	fd_set set;
	struct timeval timeout;

	FD_ZERO (&set);
	FD_SET (fd, &set);

	timeout.tv_sec = secs;
	timeout.tv_usec = 0;
	return(select(FD_SETSIZE, &set, NULL, NULL, &timeout));
}

// Insert Node in Alphabetical Order
void insertNode(struct Node* add){
	pthread_mutex_lock(&EDIT_LIST);
	if (LIST == NULL) LIST = add;
	else{
		struct Node* findEnd = LIST;
		struct Node* holdPrev = NULL;
		while(strcmp(add->ID, findEnd->ID) > 0 && findEnd->next != NULL){
			holdPrev = findEnd;
			findEnd = findEnd->next;
		}
		if (findEnd->next == NULL && strcmp(add->ID, findEnd->ID) > 0) findEnd->next = add;
		else{
			if (holdPrev == NULL){
				LIST = add;
				add->next = findEnd;
			}
			else{
				holdPrev->next = add;
				add->next = findEnd;
			}
		}
	}
	pthread_mutex_unlock(&EDIT_LIST);
}

// Free node
// -1 if error, else 1
int freeNode(struct Node* cur){
	pthread_mutex_lock(&EDIT_LIST);
	struct Node* l = LIST;
	// Free head if head
	if (l == cur){
		if (l->next == NULL){
			free(cur);
			LIST = NULL;
			cur = NULL;
		}
		else{
			LIST = cur->next;
			free(cur);
			cur = NULL;
		}
		pthread_mutex_unlock(&EDIT_LIST);
		return 1;
	}
	
	// Find Node prior to cur
	while(l!= NULL && l->next != cur) l = l->next;
	if (l == NULL) {
		pthread_mutex_unlock(&EDIT_LIST);
		return -1;
	}

	// Free node
	l->next = cur->next;
	cur->next = NULL;
	free(cur);
	cur = NULL;

	pthread_mutex_unlock(&EDIT_LIST);
	return 1;
}

// Check for Repeat Names
// 1 = Exist, 0 = Availible
int checkListName(struct Node* l, char* name){
	// Get Lowercase ver
	char lowerCheck[256];
	memset(lowerCheck, 0, sizeof(lowerCheck));
	strcpy(lowerCheck, name);
	for(int i = 0; lowerCheck[i]; ++i){
		lowerCheck[i] = tolower(lowerCheck[i]);
	}

	// Read List
	while(l != NULL){
		// Create LC ver of ID
		char lc[256];
		memset(lc, 0, sizeof(lc));
		strcpy(lc, l->ID);
		for(int i = 0; lc[i]; ++i){
			lc[i] = tolower(lc[i]);
		}

		// Check if same
		if (strcmp(lc, lowerCheck) == 0) {
			return 1;
		}

		l = l->next;
	}
	return 0;
}

// Returns NULL if empty. List otherwise
char* checkListStatus(struct Node* l){
	char array[1024];
	memset(array, 0, sizeof(array));
	strcpy(array, "");

	int i = 0;
	while(l != NULL){
		printf("NAME: %s\n", l->ID);
		if (l->status == 1){
			char temp[500];
			memset(temp, 0, sizeof(temp));
			
			sprintf(temp, "%d) %s\n", ++i, l->ID);
			strcat(array, temp);
		}
		l = l->next;
	}

	if (i == 0) return NULL;
	char* rere = array;
	return rere;
}


struct SendData getClientData(struct Node* l, char* name){
	struct SendData rere;
	rere.port = -1;

	// Get LC ver of self
	char lowerCheck[256];
	memset(lowerCheck, 0, sizeof(lowerCheck));
	strcpy(lowerCheck, name);
	for(int i = 0; lowerCheck[i]; ++i){
		lowerCheck[i] = tolower(lowerCheck[i]);
	}

	while(l != NULL){
		//printf("ID: %s\n", l->ID);
		// Set to LowerCase
		char lc[256];
		memset(lc, 0, sizeof(lc));
		strcpy(lc, l->ID);
		for(int i = 0; lc[i]; ++i){
			lc[i] = tolower(lc[i]);
		}

		if (strcmp(lowerCheck, lc) == 0){
			// Don't return data if the client isn't waiting
			if (l->status == 0) break;

			strcpy(rere.ID, l->ID);
			strcpy(rere.IP, l->IP);
			rere.port = l->port;
			break;
		}
		l = l->next;
	}
	return rere;
}

// New Thread -------------------------------------------------------------------------
void* handleNewClient(void* td){
	//printf("NEW THREAD: %ld\n", pthread_self());
	struct ThreadData* data = (struct ThreadData*)td;
	// Await request
	char recvBuffer[1024];
	int res = 0;
	while(res >= 0){
		res = input_timeout(data->client_fd, 1);
		if (res > 0){
			memset(recvBuffer, 0, sizeof(recvBuffer));
			int rec = recv(data->client_fd, recvBuffer, sizeof(recvBuffer), 0);
			if (rec < 0) {
				fprintf(stderr, "ERROR: Failed to recv from client\n");
				close(data->client_fd);
				memset(recvBuffer, 0, sizeof(recvBuffer));
				strcpy(recvBuffer, "Terminate\n\n");
			}
			//if (rec != 0) printf("RECV: %s\n", recvBuffer);

			// /list
			if(strcmp(recvBuffer, "GetList\n\n") == 0){
				char* strList = checkListStatus(LIST);
				if (strList != NULL){
					if (send(data->client_fd, strList, strlen(strList), 0) < 0) {
						fprintf(stderr, "ERROR: Failed to send to client\n");
						close(data->client_fd);
			    		memset(recvBuffer, 0, sizeof(recvBuffer));
						strcpy(recvBuffer, "Terminate\n\n");
					}
				}
				else{
					if (send(data->client_fd, "No Connections Availible\n", strlen("No Connections Availible\n"), 0) < 0) {
						fprintf(stderr, "ERROR: Failed to send to client\n");
						close(data->client_fd);
			    		memset(recvBuffer, 0, sizeof(recvBuffer));
						strcpy(recvBuffer, "Terminate\n\n");
					}
				}
			}

			// Deal with waiting status
			else if(strstr(recvBuffer, "Waiting\n\n") != NULL){
				//printf("WAITING ACT\n");
				char* getPort = recvBuffer;
				getPort += 9;

				pthread_mutex_lock(&EDIT_LIST);
				data->ClientData->status = 1;
				data->ClientData->port = (int)strtol(getPort, NULL, 10);
				pthread_mutex_unlock(&EDIT_LIST);
			}
			else if(strcmp(recvBuffer, "END_Wait\n\n") == 0){
				//printf("WAITING END\n");
				pthread_mutex_lock(&EDIT_LIST);
				data->ClientData->status = 0;
				pthread_mutex_unlock(&EDIT_LIST);
			}

			// Req for Client data by ID
			else if(strstr(recvBuffer, "ConnectTo\n\n") != NULL){
				//printf("Getting Client Data\n");
				char* temp = recvBuffer;
				temp += 11;

				struct SendData rere = getClientData(LIST, temp);
				//printf("ID Found: %s\n", rere.ID);
				if (send(data->client_fd, &rere, sizeof(rere), 0) < 0) {
					fprintf(stderr, "ERROR: Failed to send to client\n");
					close(data->client_fd);
		    		memset(recvBuffer, 0, sizeof(recvBuffer));
					strcpy(recvBuffer, "Terminate\n\n");
				}
			}

			// /quit
			else if (strcmp(recvBuffer, "Terminate\n\n") == 0){
				//printf("END BY QUIT\n");
				close(data->client_fd);
				if (freeNode(data->ClientData) == -1){
					fprintf(stderr, "ERROR: Failed to free Node\n");
		    		exit(1);
				}
				free(data);
				
				break;
			}
		}
	}
	if (res < 0){
		close(data->client_fd);
		if (freeNode(data->ClientData) == -1){
			fprintf(stderr, "ERROR: Failed to free Node\n");
    		exit(1);
		}
		free(data);
	}

	//printf("RES = %d\n", res);
	//if (res == -1) perror("\tERROR FROM RES");
	//printf("END THREAD: %ld\n", pthread_self());
	pthread_exit(NULL);
}


// Main --------------------------------------------------------------------------------
int main(int argc, char **argv){
	LIST = NULL;
	if (pthread_mutex_init(&EDIT_LIST, NULL) != 0){
		fprintf(stderr, "Error: Failed to Init mutex");
    	exit(1);
	}


	// Check Arguements
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

	// Listen for clients
	int comm_fd;
	char buffer[1024];
	pthread_t newProcess;
	while(1){
		comm_fd = accept(listen_fd, (struct sockaddr*) NULL, NULL);
		
		// Get ID
		memset(buffer, 0 , sizeof(buffer));
		if (recv(comm_fd, buffer, sizeof(buffer), 0) <= 0) {
			fprintf(stderr, "ERROR: Recv ID from Client Failed\n");
			close (comm_fd);
			close (listen_fd);
    		exit(1);
		}

		// Isolate Data
		char name[256];
		memset(name, 0, sizeof(name));
		char cip[INET_ADDRSTRLEN];
		memset(cip, 0, sizeof(cip));

		char* token = strtok(buffer, " \n");
		strcpy(name, token);
		token = strtok(NULL, "");
		strcpy(cip, token);
		

		int checkName = checkListName(LIST, name);
		
		// Send Bad Confirmation
		if (checkName == 1){
			if (send(comm_fd, "Bad ID\n\n", strlen("Bad ID\n\n"), 0) <= 0) {
				fprintf(stderr, "ERROR: Send to Confirmation to client failed\n");
				close (comm_fd);
				close (listen_fd);
				exit(1);
			}
		}
		// Send Good Confirmation continue connection
		else{
			if (send(comm_fd, "Ok ID\n\n", strlen("Ok ID\n\n"), 0) <= 0) {
				fprintf(stderr, "ERROR: Send to Confirmation to client failed\n");
				close (comm_fd);
				close (listen_fd);
				exit(1);
			}

			// Create and populate Data Struct
			struct Node* newAddress = (struct Node*)malloc(sizeof(struct Node));
			if (newAddress == NULL){
				fprintf(stderr, "ERROR: Failed to malloc new address\n");
				close (comm_fd);
				close (listen_fd);
				exit(1);
			}

			strcpy(newAddress->ID, name);
			strcpy(newAddress->IP, cip);
			newAddress->port = -1;
			newAddress->status = 0;
			newAddress->next = NULL;

			// Add to List
			insertNode(newAddress);

			// Create Thread
			struct ThreadData* td = (struct ThreadData*)malloc(sizeof(struct ThreadData));
			if (td == NULL){
				fprintf(stderr, "ERROR: Failed to malloc data struct\n");
				close (comm_fd);
				close (listen_fd);
				exit(1);
			}
			td->client_fd = comm_fd;
			td->ClientData = newAddress;

			//pthread_t newProcess;
			if (pthread_create(&newProcess, NULL, handleNewClient, td)){
				fprintf(stderr, "Error: Failed to crate a new thread\n");
				close (listen_fd);
				close(comm_fd);
				exit(1);
			}
			if (pthread_detach(newProcess)){
				fprintf(stderr, "Error: failed to detach thread");
				close(comm_fd);
				break;
			}

		}


	}

	close(listen_fd);
	pthread_mutex_destroy (&EDIT_LIST);
	exit(0);
}
