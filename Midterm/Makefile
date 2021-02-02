# Trystan Nguyen trtanguy@ucsc.edu
# 1579170
# Programming Assignment 2: Concurrent UDP Downloader

all : myclient myserver

myclient : myclient.o
	gcc -o ./myclient ./myclient.o -lm

myclient.o : 
	gcc -c -std=c99 -Wall ./myclient.c -lm

myserver : myserver.o
	gcc -o ./Server/myserver ./myserver.o -lm

myserver.o : 
	gcc -c -std=c99 -Wall ./myserver.c -lm

clean :
	rm -f myserver.o myclient.o myclient ./Server/myserver
