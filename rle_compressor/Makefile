CC     = gcc
CFLAGS = -Wall -Wextra -Wpedantic -O2 -std=c11

all: rle

rle: src/rle.c
	$(CC) $(CFLAGS) src/rle.c -o rle

clean:
	rm -f rle
