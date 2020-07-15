CC = gcc
CFLAGS= -std=c99 -D_BSD_SOURCE -Wall -ggdb -pthread

all: client server

client : client.c
	 $(CC) $(CFLAGS) -o client client.c

server : server.c
	 $(CC) $(CFLAGS) -o server server.c

clean :
	rm client server
