CC=$(CROSS_COMPILE)gcc
CFLAGS=-ggdb -Wall -pthread

all: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c

clean: 
	rm -f aesdsocket.o aesdsocket