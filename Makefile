CC = gcc
CFLAGS = -Wall -g -std=c99

all: icsh

icsh: icsh.c
	$(CC) $(CFLAGS) -o $@ $<

clean: rm -r icsh icsh.dSYM