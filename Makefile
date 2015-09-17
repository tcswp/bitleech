CC=gcc
TARGET=test
CFLAGS=-Wall -g -O0 -DDEBUG
LDFLAGS=-lm -lssl -lcrypto
OBJS=bdecode.o tracker.o metainfo.o file.o peer.o main.o

default: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(TARGET)

metainfo.o: metainfo.c
	$(CC) $(CFLAGS) -c metainfo.c
bdecode.o: bdecode.c
	$(CC) $(CFLAGS) -c bdecode.c
tracker.o: tracker.c
	$(CC) $(CFLAGS) -c tracker.c
file.o: file.c
	$(CC) $(CFLAGS) -c file.c
peer.o: peer.c
	$(CC) $(CFLAGS) -c peer.c
main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean:
	-@rm *.o $(TARGET) *~