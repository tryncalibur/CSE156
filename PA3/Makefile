# Trystan Nguyen trtanguy@ucsc.edu
# 1579170
# Programming Assignment 2: Concurrent UDP Downloader

all : myclient myserver rm

myclient : myclient.o
	gcc -o ./myclient ./myclient.o -lreadline

myclient.o : 
	gcc -c -std=c99 -Wall ./myclient.c -lreadline

myserver : myserver.o
	gcc -o ./Server/myserver ./myserver.o -lreadline

myserver.o : 
	gcc -c -std=c99 -Wall ./myserver.c -lreadline

clean :
	rm -f myserver.o myclient.o myclient ./Server/myserver

rm :
	rm -f myserver.o myclient.o
