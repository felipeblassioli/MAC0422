CC=gcc
CFLAGS=-g -Wall -I$(INCLUDE_DIR)
LDFLAGS=-lpthread

INCLUDE_DIR=include
ODIR=obj
SRCDIR=src

_OBJ=utils.o event_queue.o linked_list.o algorithms.o simulator.o
OBJ=$(patsubst %,$(ODIR)/%,$(_OBJ))

all: ep1sh ep1

ep1sh: 
	gcc src/shell.c -o ep1sh -lreadline

$(ODIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

ep1: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm ep1 ep1sh obj/*
