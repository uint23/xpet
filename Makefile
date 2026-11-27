# tools
CC = cc

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

# libs
LIBS = -lX11 -lXpm -lXext

# flags
CPPFLAGS = -D_DEFAULT_SOURCE
CFLAGS  = -std=c99 -pedantic -Wall -Wextra -Os ${CPPFLAGS} -I/usr/X11R6/include
LDFLAGS = ${LIBS} -L/usr/X11R6/lib

# files
SRC = xpet.c
BIN = xpet
MAN = xpet.1

all: ${BIN}

${BIN}: ${SRC} xpet.h config.h
	${CC} ${CFLAGS} ${SRC} -o ${BIN} ${LDFLAGS}

clean:
	rm -f ${BIN}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ${BIN} ${DESTDIR}${PREFIX}/bin/
	chmod 755 ${DESTDIR}${PREFIX}/bin/${BIN}

	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -f ${MAN} ${DESTDIR}${MANPREFIX}/man1/
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/${MAN}

uninstall:
	rm -f \
		${DESTDIR}${PREFIX}/bin/${BIN} \
		${DESTDIR}${MANPREFIX}/man1/${MAN}

clangd:
	rm -f compile_flags.txt
	for f in ${CFLAGS}; do echo $$f >> compile_flags.txt; done

.PHONY: all clean install uninstall clangd
