CC=gcc
CFLAGS=-c
SRCS=server.c deliver.c
OBJS=$(SRCS:.c=.o)
EXES=$(SRCS:.c=)

all:$(EXES)

server: server.o
	$(CC) $^ -o $@ -lpthread

deliver: deliver.o
	$(CC) $^ -o $@

$(OBJS):$(SRCS)
	$(CC) $(CFLAGS) $(SRCS)

clean:
	rm -f $(OBJS) $(EXES)

print:
	echo $(EXES)
