CC = gcc
CFLAGS = -g

OBJS = Conflenti_PA1.o 

all: Conflenti_PA1 

Conflenti_PA1.o: Conflenti_PA1.c
	$(CC) $(CFLAGS) -c Conflenti_PA1.c

Conflenti_PA1: Conflenti_PA1.o
	$(CC) $(LDFLAGS) -o $@ Conflenti_PA1.o 

clean:
	rm -f Conflenti_PA1 *.o *~ core
