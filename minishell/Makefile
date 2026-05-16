CC     = gcc
CFLAGS = -Wall -Wextra -Wpedantic -O2 -std=c11

all: minishell

minishell: shell.c
	$(CC) $(CFLAGS) shell.c -o minishell

clean:
	rm -f minishell
