#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>

int main(int argc, char **argv){
  // Check Parameters
  int getHead = -1;

  if (argc < 3 || argc > 4){
    fprintf(stderr, "ERROR: Expected 2 or 3 parameters\n");
    return -1;
  }
  if (argc == 4){
    if (strcmp(argv[3], "-h") == 0)
      getHead = 1;
    else{
      fprintf(stderr, "ERROR: Unexpected 3rd paremeter\n");
      return -1;
    }
  }

  
  // Parse parameters
  char *host = argv[1];
  char *ipS = strtok(argv[2], "/");
  char *path = strtok(NULL, ""); 
  char *ipnp = strtok(ipS, ":");
  char *portS = strtok(NULL, "");
  
  char htmlBody[1024] = {0};
  char htmlHead[1024] = {0};
  sprintf(htmlBody, "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n", path, host);
  sprintf(htmlHead, "HEAD /%s HTTP/1.1\r\nHost: %s\r\n\r\n", path, host);

  // Create and set up Socket connections
  int port = 80;
  if (strcmp(ipS, ipnp) != 0) port = atoi(portS);
  
  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof servaddr);
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);

  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(listen_fd < 0){
    fprintf(stderr, "ERROR: Socket could not be created\n");
    return -1;
  }

  int inetCheck = inet_pton(AF_INET, ipnp, &servaddr.sin_addr);
  if (inetCheck <= 0){
    fprintf(stderr, "ERROR: Invalid address\n");
    return -1;
  }
  
  
  // Connect to address
  if (connect(listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
    fprintf(stderr, "ERROR: Connection Failed\n");
    fprintf(stderr, "\t%s\n", strerror(errno));
    return -1;
  }
  
  // Get Head
  if (send(listen_fd, htmlHead, strlen(htmlHead), 0) <= 0) {
    fprintf(stderr, "ERROR: Send failed\n");
    return -1;
  }  

  char buffer[1024] = {0}; 
  int headSize = recv(listen_fd, buffer, 1023, 0);
  if (headSize < 0){
    fprintf(stderr, "ERROR: Unable to recieve\n");
    fprintf(stderr, "\t%s\n", strerror(errno));
    return -1;
  }
  if (getHead == 1) printf("%s\n", buffer);
  
  // Get Body
  else{
    char* length = strstr(buffer, "Content-Length: ");
    if (length ==NULL){
      fprintf(stderr, "ERROR: Unable to determine body length\n");
      return -1;
    }
    length = strtok(length, ": ");
    int bodySize = atoi(strtok(NULL, ""));

    if (send(listen_fd, htmlBody, strlen(htmlBody), 0) <= 0) {
      fprintf(stderr, "ERROR: Send failed\n");
      return -1;
    }
    
    bzero(buffer, 1024);
    FILE *fp;
    fp = fopen("output.dat", "w+");
    while(1){
      int inByte = recv(listen_fd, buffer, 1023, 0);
      if (inByte < 0){
	fprintf(stderr, "ERROR: Unable to Recieve\n");
	fprintf(stderr, "\t%s\n", strerror(errno));
	fclose(fp);
	return -1;
      }
      if (headSize > 0){
	if (headSize >= inByte) headSize -= inByte;
	else{
	  inByte -= headSize;
	  headSize = 0;
	  char *temp = strstr(buffer, "\r\n\r\n");
	  temp = strtok(temp, "\r\n\r\n");
	  temp = strtok(NULL, "");
	  fprintf (fp, "%s", temp);
	  
	}
      }
      if (headSize <= 0 && bodySize > 0){
	if (headSize == 0) headSize = -1;
	else fprintf(fp, "%s", buffer);
	bodySize -= inByte;
      }
      bzero(buffer, 1024);
      if (bodySize <=0) break;
    }
    fclose(fp);
  }
  
  
  close(listen_fd);
  return 0;
}
