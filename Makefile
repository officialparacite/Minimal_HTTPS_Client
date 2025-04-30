CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lssl -lcrypto

TARGETS = client client2
SOURCES = client.c client2.c

all: $(TARGETS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

client2: client2.c
	$(CC) $(CFLAGS) -o client2 client2.c $(LDFLAGS)

clean:
	rm -f $(TARGETS)
