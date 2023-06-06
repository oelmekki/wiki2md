PROG = wiki2md
CC = gcc
CFLAGS =
PREFIX = /usr/local
FILES = $(wildcard *.c)
OBJ = $(patsubst %.c, %.o, $(FILES))
OBJDEV = $(patsubst %.c, %.o-dev, $(FILES))
LIBS =

.PHONY: all dev install clean analyze

all: ${PROG}

${PROG}: ${OBJ}
	${CC} ${KIK_PROD_CFLAGS} ${CFLAGS} $^ -o ${PROG} ${LIBS}

%.o: %.c
	${CC} ${KIK_PROD_CFLAGS} ${CFLAGS} -c $< -o $@

dev: ${PROG}-dev
	ctags --kinds-C=+p ${FILES} *.h $(shell project_headers ${CFLAGS} ${LIBS})

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


