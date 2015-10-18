all: hmac.o

CFLAGS += -fPIC

hmac.o: sha2.o hmac_sha2.o
	ld -r $^ -o $@

#hmac_sha2.c hmac_sha2.h sha2.c sha2.h
#	$(CC) -Wall -c -fPIC hmac_sha2.c sha2.c -o hmac.o

clean:
	rm -f *.o

