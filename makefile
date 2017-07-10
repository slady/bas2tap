
CC=gcc
CFLAGS="-Wall"

.PHONY : test all clean %.test

all: bas2tap

bas2tap: bas2tap.o

bas2tap.o: bas2tap.c

clean:
	rm -f bas2tap.o bas2tap

all-tests := $(addsuffix .test, $(basename $(wildcard test/*.tap)))

test: clean all $(all-tests)

%.test: %.tap %.bas
	./bas2tap $(basename $(notdir $(word 1, $?))) $(word 2, $?) | diff -q $(word 1, $?) - >/dev/null || (echo "Test $@ failed" && exit 1)
