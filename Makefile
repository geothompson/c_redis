CC = g++

CFLAGS = -g -Wall -Wextra -g -o2

all: server client

server:
	$(CC) server.cpp $(CFLAGS) -o server

client:
	$(CC) client.cpp $(CFLAGS) -o client

clean:
	rm -f server client


