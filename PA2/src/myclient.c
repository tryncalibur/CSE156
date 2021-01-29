#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/time.h>
#include <math.h>

// Message architecture
struct Message{
    char type[10];
    int chunk;
    int chunkTotal;
    int chunklet;
    int chunkletTotal;
    int length;
    char data[1024];
};

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

// Use to send a request to a server for a chunk
// Use after confirming server has file
int getChunkFromServer(char* fileName, int c, int cNum, int cl, int clNum, int fS, int sfd, struct sockaddr_in addr){
  struct Message getChunk;
  strcpy(getChunk.type, "Request");
  strcpy(getChunk.data, fileName);
  getChunk.chunk = c;
  getChunk.chunkTotal = cNum;
  getChunk.chunklet = cl;
  getChunk.chunkletTotal = clNum;
  getChunk.length = fS;
  int sent = sendto(sfd, (const struct Message*)&getChunk, sizeof(getChunk), 0, 
                    (const struct sockaddr*)&addr, sizeof(addr));
  if (sent < 0){
    return -1;
  }
  return 1;
}

// Seperate filename from path
char* getFileName(char* filePath){
  char* first;
  char* second;
  first = strtok(filePath, "/");
  second = strtok(NULL, "/");
  while(second != NULL){
    first = second;
    second = strtok(NULL, "/");
  }
  return first;
}


//-----------------------------------------------------------------------------------------------------------------------
int main(int argc, char* argv[]){
  // Check runtime arguements
  if (argc != 4){
    fprintf(stderr, "Usage: ./myclient <server-info.txt> <num-chunk> <filename-path>\n");
    exit(1);
  }

  if (access (argv[1], F_OK) != 0){
    fprintf(stderr, "Error: ServerList does not exist\n");
    exit(1);
  }

  int numchunks = atoi(argv[2]);
  if (numchunks <= 0){
    fprintf(stderr, "Error: <num-chunks> must be a valid integer\n");
    exit(1);
  }

  
  // Create Socket FD
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0){
    fprintf(stderr, "Error: Unable to create socket\n");
    exit(1);
  }


  //------------------------------------------------------------------------------------------------------------------
  // Check Servers Avaliability
  int fileSize = 0;
  FILE *serverList = fopen(argv[1], "rb");
  if (serverList == NULL){
    fprintf(stderr, "Error: Unable to read file\n");
    close(sockfd);
    exit(1);
  }
  
  int numberOfServers = 0;
  char line [256];
  while(fgets(line, sizeof(line), serverList)){
    ++numberOfServers;
  }
  rewind(serverList);
  
  // Dertermine Server's Existance
  struct sockaddr_in servers[numberOfServers];
  int current = 0;
  while(fgets(line, sizeof(line), serverList)){
    char* ip = strtok(line, " \t");
    char* port = strtok(NULL, " \t");
    if (ip == NULL || port == NULL) continue;
    
    // Create Temp Server struct
    struct sockaddr_in temp;
    memset(&temp, 0, sizeof(temp));
    temp.sin_family = AF_INET;
    temp.sin_port = htons(atoi(port));
    if (inet_pton(AF_INET, ip, &temp.sin_addr.s_addr) <= 0){
      fprintf(stderr, "Error: <server-info> contains unformatted IP\n");
      continue;
    }
    
    // Check for IP and Port repetition
    int j = 0;
    for(int i = 0; i < current; ++i){
      if (temp.sin_addr.s_addr == servers[i].sin_addr.s_addr){
        if (temp.sin_port == servers[i].sin_port) {
          ++j;
          break;
        }
      }
    }
    if (j > 0) continue;
    
    // Check if Server has file
    struct Message check;
    strcpy(check.type, "Check");
    strcpy(check.data, argv[3]);
    check.length = strlen(argv[3]);

    int counter = 0;
    int res = 0;
    while(res >=0){
      int sent = sendto(sockfd, (const struct Message*)&check, sizeof(check), 0, 
                      (const struct sockaddr*)&temp, sizeof(temp));
      if (sent < 0){
        fprintf(stderr, "Error: Unable to send check\n");
        close(sockfd);
        fclose(serverList);
        exit(1);
      }
      res = input_timeout(sockfd, 2);

      // Check Message Recieved
      if (res > 0){
        struct Message recvMsg;
        int len = sizeof(temp);
        int recv = recvfrom(sockfd, (struct Message*)&recvMsg, sizeof(recvMsg), 0, (struct sockaddr*)&temp, &len);
        if (recv < 0){
          fprintf(stderr, "Error: Unable to recieve check response\n");
          close(sockfd);
          fclose(serverList);
          exit(1);
        }
        
        // Save Server info if file exits
        if (strcmp(recvMsg.type, "Yes") == 0){
          if (fileSize != 0){
              if (recvMsg.length != fileSize){
                fprintf(stderr, "Error: FileSize Difference between servers\n");
                close(sockfd);
                fclose(serverList);
                exit(1);
              }
          }
          servers[current] = temp;
          fileSize = recvMsg.length;
          ++current;
          break;
        }
        else if (strcmp(recvMsg.type, "No") == 0){
          break;
        }
        break;
      }

      // End Connection after 5 failed attempts
      else{
        ++counter;
        if (counter >= 5) break;
        sent = sendto(sockfd, (const struct Message*)&check, sizeof(check), 0, 
                      (const struct sockaddr*)&temp, sizeof(temp));
        if (sent < 0){
          fprintf(stderr, "Error: Unable to send check\n");
          close(sockfd);
          fclose(serverList);
          exit(1);
        }
      }

    }
  }
  fclose(serverList);
  //------------------------------------------------------------------------------------------------------------------
  // End if no Servers are avaliable
  if (current == 0){
    fprintf(stderr, "Error: Servers do not satisfy prerequisite\n");
    close(sockfd);
    exit(1);
  }
  // Create empty file and end if filesize is 0
  if (fileSize == 0){
    FILE* output = fopen(getFileName(argv[3]), "wb");
    if (output == NULL){
      fprintf(stderr, "Error: Unable to write new file\n");
      close(sockfd);
      exit(1);
    }
    fclose(output);
    exit(0);
  }

  //------------------------------------------------------------------------------------------------------------------
  // Initialize file placeholder
  int chunkSize = (int) ceil((double)fileSize / (double)numchunks);
  int chunkletRead[numchunks];
  memset(&chunkletRead, 0, sizeof(chunkletRead));
  int numChunklets = (int) ceil((double)chunkSize/1024.0);

  char fileToString[numchunks][chunkSize+numChunklets+1];
  for (int i = 0; i < numchunks; ++i){
     strcpy(fileToString[i], "");
  }
  // Assign chunk to server
  int chunkGetServer[numchunks];
  for(int i = 0; i < numchunks; ++i){
    chunkGetServer[i] = i % current;
  }


  // Get File chunks from Servers
  int res = 0;
  int deadlock = 0;
    while(res >=0){
      res = input_timeout(sockfd, 4);

      // Recieve message from any port
      if (res > 0){
        struct Message recvChunk;
        int recv = recvfrom(sockfd, (struct Message*)&recvChunk, sizeof(recvChunk), 0, NULL , NULL);
        if (recv < 0){
          fprintf(stderr, "Error: Unable to recieve check response\n");
          close(sockfd);
          exit(1);
        }

        // Determine if Message is relevent
        if (strcmp(recvChunk.type, "Chunk") == 0){
          if (recvChunk.chunklet == chunkletRead[recvChunk.chunk]) {
            deadlock = 0;
            char* trun = recvChunk.data;
            ++trun;
            strcat(fileToString[recvChunk.chunk], trun);
            chunkletRead[recvChunk.chunk] = recvChunk.chunklet + 1;

            // Get next chunk if current chunk is done transfering
            if (chunkletRead[recvChunk.chunk] == numChunklets){
              fileToString[recvChunk.chunk][chunkSize+numChunklets] = '\0';
              int currentServer = chunkGetServer[recvChunk.chunk];
              chunkGetServer[recvChunk.chunk] = -2;

              // Find and send Message to get new chunk
              for (int i = 0; i < numchunks; ++i){
                if (chunkGetServer[i] == -1){
                  chunkGetServer[i] = currentServer;
                  int x = getChunkFromServer(argv[3], i, numchunks, chunkletRead[i], numChunklets, 
                                            fileSize, sockfd, servers[chunkGetServer[i]]);
                  if (x < 0){
                    fprintf(stderr, "Error: Unable to send Request for chunk\n");
                    close(sockfd);
                    exit(1);
                  }
                  break;
                }
              }
            }

            // Send to get new chunklet from same chunk
            else{
              int i = recvChunk.chunk;
              int x = getChunkFromServer(argv[3], i, numchunks, chunkletRead[i], numChunklets, fileSize, 
                                        sockfd, servers[chunkGetServer[i]]);
              if (x < 0){
                fprintf(stderr, "Error: Unable to send Request for chunk\n");
                close(sockfd);
                exit(1);
              }
            }
          }
        }
      }

      // Send Messages to all servers to get respective chunk after deadlock
      else{
        // Handle Dead Server
        if (deadlock > current){
          for(int i = 0; i < numchunks; ++i){
            if (chunkGetServer[i] >= 0) chunkGetServer[i]++;
            if (chunkGetServer[i] >= current) chunkGetServer[i] = 0;
          }
        }
        if (deadlock > 2*current){
          fprintf(stderr, "Error: All servers ended connection\n");
          close(sockfd);
          exit(1);
        }
        // Resend Requests
        int isDone = 0;
        for (int i = 0; i < numchunks; ++i){
          if (chunkGetServer[i] >= 0){
            int x = getChunkFromServer(argv[3], i, numchunks, chunkletRead[i], numChunklets, fileSize, 
                                      sockfd, servers[chunkGetServer[i]]);
            if (x < 0){
              fprintf(stderr, "Error: Unable to send Request for chunk\n");
              close(sockfd);
              exit(1);
            }
          }
          else ++isDone; 
        }
        if (isDone == numchunks) break;
        ++deadlock;
      }
    }
    close(sockfd);
//------------------------------------------------------------------------------------------------------------------


    
    // Write to File
    FILE* output = fopen(getFileName(argv[3]), "wb");
    if (output == NULL){
      fprintf(stderr, "Error: Unable to write new file\n");
      exit(1);
    }
    for(int i = 0 ; i < numchunks; ++i){
      fprintf(output, "%s", fileToString[i]);
    }
    fclose(output);
}
