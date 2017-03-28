CC=gcc
CFLAGS=-g -c
LIBS=-lm -lpthread
SRCS=server.c client.c
OBJS=$(SRCS:.c=.o)

all:server client

server:server.o
	$(CC) -o server server.o $(LIBS)

client:client.o
	$(CC) -o client client.o

$(OBJS):$(SRCS)
	$(CC) $(CFLAGS) $(SRCS)



clean:
	rm -f *.o server client
