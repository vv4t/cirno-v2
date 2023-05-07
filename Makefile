cli_demo=$(wildcard demo_cli/*.9c)
sdl_demo=$(wildcard demo_sdl/*.9c)

.PHONY=build demo run $(cli_demo) $(sdl_demo)

build: run

demo: cirno $(cli_demo) $(sdl_demo)

cirno: src/*.c src/*.h
	gcc src/*.c -lm -lSDL2 -g -o cirno

run: cirno
	./cirno main.9c

demo_cli/%:
	./cirno $@

demo_sdl/%:
	./cirno -w $@
