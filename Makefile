DESTDIR?=/usr/local

DESTDIR_BIN:=$(DESTDIR)/bin
DESTDIR_LIB:=$(DESTDIR)/lib/ucw

all: ucw lib32ucw.so lib64ucw.so

ucw: ucw.c
	$(CC) -O ucw.c -o ucw

lib64ucw.so: libucw.c
	$(CC) -m64 -O -Wall -shared -fPIC libucw.c -o $@ -ldl

lib32ucw.so: libucw.c
	$(CC) -m32 -O -Wall -shared -fPIC libucw.c -o $@ -ldl

install: all
	[ -n "$(DESTDIR)" ] || { echo Please specify DESTDIR; exit 1; }
	[ -d "$(DESTDIR)" ] || { echo "$(DESTDIR)" does not exist; exit 1; }
	mkdir -p "$(DESTDIR_BIN)" "$(DESTDIR_LIB)" "$(DESTDIR_LIB)/lib32/" "$(DESTDIR_LIB)/lib64" "$(DESTDIR_LIB)/lib/i386-linux-gnu/" "$(DESTDIR_LIB)/lib/x86_64-linux-gnu/"
	cp -v ucw "$(DESTDIR_BIN)"
	cp -v lib32ucw.so lib64ucw.so "$(DESTDIR_LIB)"
	ln -sfv ../lib32ucw.so "$(DESTDIR_LIB)/lib32/libucw.so"
	ln -sfv ../lib64ucw.so "$(DESTDIR_LIB)/lib64/libucw.so"
	# Debian based systems need this
	ln -sfv ../../lib32ucw.so "$(DESTDIR_LIB)/lib/i386-linux-gnu/libucw.so"
	ln -sfv ../../lib64ucw.so "$(DESTDIR_LIB)/lib/x86_64-linux-gnu/libucw.so"
	if [ "$$(uname -m)" = "x86_64" ] ;\
	then \
		ln -sfv ../lib64ucw.so "$(DESTDIR_LIB)/lib/libucw.so" ;\
	else \
		ln -sfv ../lib32ucw.so "$(DESTDIR_LIB)/lib/libucw.so" ;\
	fi

clean:
	rm -f lib32ucw.so lib64ucw.so *.o ucw
