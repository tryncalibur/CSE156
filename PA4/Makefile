# Trystan Nguyen trtanguy@ucsc.edu
# 1579170
# Programming Assignment 4

all : proxy rm

proxy : proxy.o
	gcc -o ./bin/proxy ./proxy.o -lpthread

proxy.o : 
	gcc -c -std=c99 -Wall ./src/proxy.c -lpthread

clean :
	rm -f proxy.o ./bin/proxy 

rm :
	rm -f proxy.o 
