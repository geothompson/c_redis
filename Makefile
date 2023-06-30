CC = g++

CFLAGS = -g -o2
# CFLAGS = -g -Wall -Wextra -g -o2

all: server client

source: sserver sclient

server:
	$(CC) server.cpp $(CFLAGS) -o server

client:
	$(CC) client.cpp $(CFLAGS) -o client

sserver:
	$(CC) 07_server.cpp $(CFLAGS) -o server

sclient:
	$(CC) 07_client.cpp $(CFLAGS) -o client

clean:
	rm -f server client

build:
		$(MAKE) clean
		$(MAKE) all


book:
		$(MAKE) clean
		$(MAKE) source


