
CC=gcc
CFLAGS=-Wall

.PHONY : test all clean %.test

all: bas2tap

bas2tap: bas2tap.o
	$(CC) bas2tap.o -o bas2tap

bas2tap.o: bas2tap.c
	$(CC) $(CFLAGS) -c bas2tap.c -o bas2tap.o

clean:
	rm -f bas2tap.o bas2tap

all-tests := $(addsuffix .test, $(basename $(wildcard test/*.tap)))

test: clean all $(all-tests)
	@echo "- [ All tests passed OK! ] -"

%.test: %.tap %.bas
	@./bas2tap $(basename $(notdir $(word 1, $?))) $(word 2, $?) | diff -q $(word 1, $?) - >/dev/null || (echo "Test $@ failed" && exit 1)
