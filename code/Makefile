# Makefle for UDP socket examples
#
# Saleem Bhatti <https://saleem.host.cs.st-andrews.ac.uk/>
# January 2022
# January 2021

CC		=clang
#CC	=gcc
CC-flags		=-Wall #-g

MATH	=-lm

LIB-files	=CrudpSocket.o

PROGRAMS	=Crudp

.SUFFIXES:	.c .o

.c.o:;	$(CC) $(CC-flags) -c $< 

C-files		=CrudpSocket.c \
	timer.c \
	Crudp.c

O-files		=$(C-files:%.c=%.o)

all:	$(PROGRAMS)


CrudpSocket.c:	CrudpSocket.h

Crudp.c:	CrudpSocket.h

timer:	timer.o
	$(CC) -o $@ $+

Crudp:	Crudp.o $(LIB-files)
	$(CC) -o $@ $+ $(MATH)

.PHONY:	clean

clean:;	rm -rf *.o $(PROGRAMS) *~
