.PHONY=build run

build: cirno run

cirno: src/*.c src/*.h
	gcc src/*.c -g -o cirno

run:
	./cirno main.cn
