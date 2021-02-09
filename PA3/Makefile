# Trystan Nguyen trtanguy@ucsc.edu
# 1579170
# Programming Assignment 3: Simple Remote Server

all : myclient myserver rm

myclient : myclient.o
	gcc -o ./bin/myclient ./myclient.o -lreadline 

myclient.o : 
	gcc -c -std=c99 -Wall ./src/myclient.c -lreadline

myserver : myserver.o
	gcc -o ./bin/myserver ./myserver.o -lreadline

myserver.o : 
	gcc -c -std=c99 -Wall ./src/myserver.c -lreadline

clean :
	rm -f myserver.o myclient.o myclient myserver

rm :
	rm -f myserver.o myclient.o
