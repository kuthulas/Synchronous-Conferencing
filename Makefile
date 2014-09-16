CC = gcc
CFLAGS=-Wall -lpthread
all: server client
 server: server.c SBCP.h
		$(CC) $(CFLAGS) server.c -o server

 client: client.c SBCP.h
		$(CC) $(CFLAGS)  client.c -o client 

 clean:
	rm -rf *o server client
