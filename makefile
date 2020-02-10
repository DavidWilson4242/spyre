CC = gcc
CF = -std=c11 -Wno-format -g -O2
COMPILE_OBJ = build/main.o build/lex.o build/parse.o build/hash.o build/gc.o build/asm.o build/spyre.o build/memory.o build/gen.o

clean:
	rm -Rf build/*.o

spyre: build $(COMPILE_OBJ)
	$(CC) $(CF) $(COMPILE_OBJ) -o spyre

build:
	mkdir build

build/lex.o:
	$(CC) $(CF) -c src/lex.c -o build/lex.o

build/parse.o:
	$(CC) $(CF) -c src/parse.c -o build/parse.o

build/main.o:
	$(CC) $(CF) -c src/main.c -o build/main.o

build/hash.o:
	$(CC) $(CF) -c src/hash.c -o build/hash.o

build/spyre.o:
	$(CC) $(CF) -c src/spyre.c -o build/spyre.o

build/gc.o:
	$(CC) $(CF) -c src/gc.c -o build/gc.o

build/asm.o:
	$(CC) $(CF) -c src/asm.c -o build/asm.o

build/memory.o:
	$(CC) $(CF) -c src/memory.c -o build/memory.o

build/gen.o:
	$(CC) $(CF) -c src/gen.c -o build/gen.o
