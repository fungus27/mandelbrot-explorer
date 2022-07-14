CC = gcc
LDFLAGS := -lGL -lglfw -lGLEW
main: main.o

clean:
	rm *.o main

.PHONY: clean
