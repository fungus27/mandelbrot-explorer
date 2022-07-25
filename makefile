CC = gcc
LDFLAGS := -lm -lpthread -lGL -lglfw -lGLEW
main: main.o

clean:
	rm *.o main

.PHONY: clean
