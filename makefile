CC = gcc
CF = -std=c11 -g -O2
COMPILE_OBJ = build/main.o build/lex.o build/parse.o
VM_OBJ = build/spyre.o build/vmtest.o build/hash.o

clean:
	rm -Rf build/*.o

spyre: build $(COMPILE_OBJ)
	$(CC) $(CF) $(COMPILE_OBJ) -o spyre

vm: build $(VM_OBJ)
	$(CC) $(CF) $(VM_OBJ) -o vm

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

build/vmtest.o:
	$(CC) $(CF) -c src/vmtest.c -o build/vmtest.o

build/spyre.o:
	$(CC) $(CF) -c src/spyre.c -o build/spyre.o
