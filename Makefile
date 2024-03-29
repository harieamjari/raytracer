CC = clang
#CFLAGS = -fsanitize=address -fsanitize=undefined -fopenmp -O3 -ggdb3 -Wall -Wextra
CFLAGS =  -ggdb3 -O3 -fopenmp
LDFLAGS = -lm
main : utils.c math.c main.c tracer.c
ppm : utils.c math.c ppm.c tracer.c

all : main ppm
	mkdir bin
	mv $^ bin

clean :
	rm main ppm
