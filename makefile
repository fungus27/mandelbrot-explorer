CC = gcc
LDFLAGS := -lm -lpthread -lGL -lglfw -lGLEW -lavutil -lavcodec -lavformat -g
CFLAGS := -g
OBJ := main.o record.o

main: $(OBJ)

$(OBJ): record.h

clean:
	rm *.o main

.PHONY: clean
