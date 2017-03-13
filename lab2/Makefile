CC=gcc
CFLAGS=-g -c
LIBS=-lm -lpthread
SRCS=server.c
OBJS=$(SRCS:.c=.o)

all:$(OBJS)
	$(CC) -o server server.o $(LIBS)
	#$(CC) -o client client.o

$(OBJS):$(SRCS)
	$(CC) $(CFLAGS) $(SRCS)



clean:
	rm -f *.o server
