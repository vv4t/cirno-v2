cli_demo=$(wildcard demo/cli/*.9c)
sdl_demo=$(wildcard demo/sdl/*.9c)

.PHONY=build demo run $(cli_demo) $(sdl_demo)

build: run

demo: cirno $(cli_demo) $(sdl_demo)

cirno: src/*.c src/*.h
	gcc src/*.c -lm -lSDL2 -g -o cirno

run: cirno
	./cirno demo/main.9c

demo/cli/%:
	./cirno $@

demo/sdl/%:
	./cirno -w $@
