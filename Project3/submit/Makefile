CC = gcc
CFLAGS= -std=c99 -D_BSD_SOURCE -Wall -ggdb -pthread

votecounter : votecounter.c
	 $(CC) $(CFLAGS) -o votecounter votecounter.c

clean :
	rm votecounter
