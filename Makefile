LIBEV=libev-4.19

CFLAGS=-c -g -O0 -I$(LIBEV)
CPPFLAGS=-c -std=c++11 -g -O0 -I$(LIBEV)

all: server

libev.o: $(LIBEV)/*
	g++ -o libev.o $(CFLAGS) -w libev.c

server.o: server.cc
	g++ -o server.o $(CPPFLAGS) server.cc

server: libev.o server.o
	g++ -o server $(LFLAGS) libev.o server.o

