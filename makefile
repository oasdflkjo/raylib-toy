CC=gcc
CFLAGS=-I./external/raylib/src
LDFLAGS=-L./external/raylib/src -lraylib

main: src/main.c
	$(CC) $(CFLAGS) src/main.c -o main $(LDFLAGS)