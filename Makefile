.PHONY=build run

build: cirno run

cirno: src/*.c src/*.h
	gcc src/*.c -o cirno

run:
	./cirno
