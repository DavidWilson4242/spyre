CC = gcc
CF = -std=c11 -g -O2
OBJ = build/main.o build/lex.o build/parse.o

all: spyre

clean:
	rm -Rf build/*.o

spyre: build $(OBJ)
	$(CC) $(CF) $(OBJ) -o spyre

build:
	mkdir build

build/lex.o:
	$(CC) $(CF) -c src/lex.c -o build/lex.o

build/parse.o:
	$(CC) $(CF) -c src/parse.c -o build/parse.o

build/main.o:
	$(CC) $(CF) -c src/main.c -o build/main.o
