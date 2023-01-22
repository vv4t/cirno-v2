.PHONY=build cli sdl

build: cli sdl

cirno_cli: src/cirno/*.c src/cirno_cli.c src/cirno/*.h
	gcc src/cirno/*.c src/cirno_cli.c -g -o cirno_cli

cirno_sdl: src/cirno/*.c src/cirno_sdl.c src/cirno/*.h
	gcc src/cirno/*.c src/cirno_sdl.c -lm -lSDL2 -g -o cirno_sdl

cli: cirno_cli
	./cirno_cli cli.cn

sdl: cirno_sdl
	./cirno_sdl sdl.cn
