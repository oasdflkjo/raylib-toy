CC = gcc
CFLAGS = -I./external/raylib/src -I./include -msse4.1 -Ofast 
LDFLAGS = -L./external/raylib/src -Wall -lraylib -lopengl32 -lgdi32 -lwinmm -luser32 -lshell32 -lm -lpthread -mavx2 

all: raylib main

raylib:
	cd external/raylib/src && make

main: src/main.c src/threadpool.c include/threadpool.h
	$(CC) $(CFLAGS) src/main.c src/threadpool.c $(LDFLAGS) -o main

clean:
	rm -f main
