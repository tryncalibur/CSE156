
all : myclient myserver rmo

myclient : myclient.o
	gcc -o ./bin/myclient ./myclient.o

myclient.o : 
	gcc -c -std=c99 -Wall ./src/myclient.c

myserver : myserver.o
	gcc -o ./bin/Server/myserver ./myserver.o

myserver.o : 
	gcc -c -std=c99 -Wall ./src/myserver.c

rmo : 
	rm -f ./myclient.o ./myserver.o

clean :
	rm -f ./myclient.o ./myserver.o ./bin/myclient ./bin/Server/myserver