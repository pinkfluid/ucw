all: ucw libucw.so

ucw: ucw.c
	$(CC) -O ucw.c -o ucw

libucw.so: libucw.c
	$(CC) -O -Wall -shared -fPIC libucw.c -o libucw.so -ldl

clean: 
	rm -f libucw.so ucw.o ucw
