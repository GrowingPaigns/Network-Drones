# Makefile for drone

CC = gcc
OBJCS = drone8.c
 
# setup for system
LIBS = -lm

all: drone8

drone8: $(OBJCS)
	$(CC) -o $@ $(OBJCS) $(LIBS)

clean:
	rm drone8