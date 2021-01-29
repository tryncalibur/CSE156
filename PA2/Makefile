# Trystan Nguyen trtanguy@ucsc.edu
# 1579170
# Programming Assignment 2: Concurrent UDP Downloader

all : myclient myserver rmo

myclient : myclient.o
	gcc -o ./bin/myclient ./myclient.o -lm

myclient.o : 
	gcc -c -std=c99 -Wall ./src/myclient.c -lm

myserver : myserver.o
	gcc -o ./bin/Server/myserver ./myserver.o -lm

myserver.o : 
	gcc -c -std=c99 -Wall ./src/myserver.c -lm

rmo : 
	rm -f ./myclient.o ./myserver.o