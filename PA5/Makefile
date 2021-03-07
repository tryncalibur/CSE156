# Trystan Nguyen trtanguy@ucsc.edu
# 1579170
# Programming Assignment 5: Simple chat

all : client server rm

client : client.o
	gcc -o ./bin/client ./client.o -lpthread

client.o : 
	gcc -c -std=c99 -Wall ./src/client.c -lpthread

server : server.o
	gcc -o ./bin/server ./server.o -lpthread

server.o : 
	gcc -c -std=c99 -Wall ./src/server.c -lpthread

clean :
	rm -f server.o client.o client server

rm :
	rm -f server.o client.o
