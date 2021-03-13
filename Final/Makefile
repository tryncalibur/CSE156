
all : myclient myserver

myclient : myclient.o
	gcc -o ./myclient ./myclient.o

myclient.o : 
	gcc -c -std=c99 -Wall ./myclient.c

myserver : myserver.o
	gcc -o ./myserver ./myserver.o

myserver.o : 
	gcc -c -std=c99 -Wall ./myserver.c

clean :
	rm -f ./myclient.o ./myserver.o ./myclient ./myserver