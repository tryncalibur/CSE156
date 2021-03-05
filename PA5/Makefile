# Trystan Nguyen trtanguy@ucsc.edu
# 1579170
# Programming Assignment 5: Simple chat

all : myclient myserver rm

myclient : myclient.o
	gcc -o ./bin/myclient ./myclient.o -lpthread

myclient.o : 
	gcc -c -std=c99 -Wall ./src/myclient.c -lpthread

myserver : myserver.o
	gcc -o ./bin/myserver ./myserver.o -lpthread

myserver.o : 
	gcc -c -std=c99 -Wall ./src/myserver.c -lpthread

clean :
	rm -f myserver.o myclient.o myclient myserver

rm :
	rm -f myserver.o myclient.o
