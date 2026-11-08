CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2

smallsh: smallsh.c
	$(CC) $(CFLAGS) smallsh.c -o smallsh

clean:
	rm -f smallsh

.PHONY: clean
