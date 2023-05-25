# Makefile for client and server

CC = gcc
OBJCS = HilferClient.c
OBJCSS = HilferServer.c

CFLAGS =  -g -Wall
# setup for system
nLIBS =

all: HilferClient HilferServer

client0: $(OBJCS)
	$(CC) $(CFLAGS) -o $@ $(OBJCS) $(LIBS)

server0: $(OBJCSS)
	$(CC) $(CFLAGS) -o $@ $(OBJCSS) $(LIBS)


clean:
	rm HilferClient HilferServer
