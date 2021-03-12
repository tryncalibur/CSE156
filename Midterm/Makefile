# Trystan Nguyen trtanguy@ucsc.edu
# 1579170
# Midterm

all : myclient myserver rmf

myclient : myclient.o
	gcc -o ./myclient ./myclient.o

myclient.o : 
	gcc -c -std=c99 -Wall ./myclient.c

myserver : myserver.o
	gcc -o ./myserver ./myserver.o

myserver.o : 
	gcc -c -std=c99 -Wall ./myserver.c

clean :
	rm -f myserver.o myclient.o myclient ./myserver

rmf :
	rm -f myserver.o myclient.o
