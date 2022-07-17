CC = gcc
LDFLAGS := -lm -lGL -lglfw -lGLEW
main: main.o

clean:
	rm *.o main

.PHONY: clean
