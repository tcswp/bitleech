bdecode:
	gcc -Wall -g -O0 -pthread -lm -lssl -lcrypto bdecode.c tracker.c metainfo.c file.c peer.c main.c -o test
clean:
	rm test
