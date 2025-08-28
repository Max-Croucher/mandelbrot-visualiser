CC=gcc
CFLAGS= -O4 -Werror -Wall
LIBS= -lm -lpng -pthread

all: mandelbrot

mandelbrot.o: mandelbrot.c
	$(CC) -c $(CFLAGS) $< -o $@ $(LIBS)

mandelbrot: mandelbrot.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

# Target: clean project.
.PHONY: clean
clean: 
	-$(DEL) *.o
