.PHONY=build run

build: run

cirno: src/*.c src/*.h
	gcc src/*.c -lm -lSDL2 -g -O9 -o cirno

run: cirno
	./cirno -w main.9c
