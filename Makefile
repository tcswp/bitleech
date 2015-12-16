CC=gcc
TARGET=test
CFLAGS=-Wall -ggdb -DDBG
LDFLAGS=-lm -lssl -lcrypto
OBJS=bdecode.o tracker.o queue.o file.o peer.o main.o

default: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(TARGET)

bdecode.o: bdecode.c
	$(CC) $(CFLAGS) -c bdecode.c
tracker.o: tracker.c peer.c
	$(CC) $(CFLAGS) -c tracker.c
file.o: file.c
	$(CC) $(CFLAGS) -c file.c
peer.o: peer.c
	$(CC) $(CFLAGS) -c peer.c
queue.o: queue.c
	$(CC) $(CFLAGS) -c queue.c
main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean:
	-@rm *.o $(TARGET) *~
