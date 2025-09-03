CC=gcc
CFLAGS= -O4 -Werror -Wall
LIBS= -lm -lpng -pthread

all: mandelbrot julia-explore

mandelbrot.o: mandelbrot.c
	$(CC) -c $(CFLAGS) $< -o $@ $(LIBS)

mandelbrot: mandelbrot.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)

julia-explore.o: julia_explore.c
	$(CC) -c $(CFLAGS) $< -o $@ $(LIBS) -lglut -lGL

julia-explore: julia_explore.o
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS) -lglut -lGL

# Target: clean project.
.PHONY: clean
clean: 
	-$(DEL) *.o
