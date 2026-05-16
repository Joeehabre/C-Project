CC     = gcc
CFLAGS = -Wall -Wextra -Wpedantic -O2 -std=c11

all: http_server

http_server: http_server.c
	$(CC) $(CFLAGS) http_server.c -o http_server

clean:
	rm -f http_server
