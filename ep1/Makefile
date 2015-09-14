CC=gcc
CFLAGS=-g -Wall -I$(INCLUDE_DIR)
LDFLAGS=-lpthread

INCLUDE_DIR=include
ODIR=obj
SRCDIR=src

_OBJ=utils.o event_queue.o linked_list.o scheduler.o algorithm.o simulator.o
OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))

all: ep1sh ep1

ep1sh: 
	gcc src/shell.c -o ep1sh -lreadline

$(ODIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

ep1: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean pack
clean:
	rm ep1 ep1sh obj/*

pack:
	mkdir ep1-felipe_7991058
	mkdir -p ep1-felipe_7991058/obj
	cp -r src/ ep1-felipe_7991058/
	cp -r include/ ep1-felipe_7991058/
	cp Makefile ep1-felipe_7991058/
	cp LEIAME ep1-felipe_7991058/
	cp apresentacao.pdf ep1-felipe_7991058/
	tar -pczf ep1-felipe_7991058.tar.gz ep1-felipe_7991058
	rm -rf ep1-felipe_7991058/
