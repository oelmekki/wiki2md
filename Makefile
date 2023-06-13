PROG = wiki2md
CC = gcc
CFLAGS =
PREFIX = /usr/local
FILES = $(wildcard *.c)
OBJ = $(patsubst %.c, %.o, $(FILES))
OBJDEV = $(patsubst %.c, %.o-dev, $(FILES))
LIBS =
KIK_DEV_CFLAGS = -std=c18 -D_POSIX_C_SOURCE=200809L -O0 -Wall -Wextra -Wpedantic -Wformat=2 -Werror -g3 -ggdb3 -fsanitize=undefined -fsanitize=address -fsanitize=pointer-compare
KIK_PROD_CFLAGS = -std=c18 -D_POSIX_C_SOURCE=200809L -O2 -pipe -march=native

.PHONY: all dev install clean analyze

all: ${PROG}

${PROG}: ${OBJ}
	${CC} ${KIK_PROD_CFLAGS} ${CFLAGS} $^ -o ${PROG} ${LIBS}

%.o: %.c
	${CC} ${KIK_PROD_CFLAGS} ${CFLAGS} -c $< -o $@

dev: ${PROG}-dev
	ctags --kinds-C=+p ${FILES} *.h $(shell ./project_headers ${CFLAGS} ${LIBS})

${PROG}-dev: ${OBJDEV}
	${CC} ${KIK_DEV_CFLAGS} ${CFLAGS} $^ -o ${PROG}-dev ${LIBS}

%.o-dev: %.c
	${CC} ${KIK_DEV_CFLAGS} ${CFLAGS} -c $< -o $@

install: ${PROG}
	install -D ${PROG} ${PREFIX}/bin/${PROG}

clean:
	rm -f ${PROG} ${PROG}-dev *.o *.o-dev

analyze:
	scan-build clang ${KIK_PROD_CFLAGS} ${CFLAGS} ${FILES} -o /dev/null ${LIBS}


