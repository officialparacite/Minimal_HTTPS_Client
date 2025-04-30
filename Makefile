CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lssl -lcrypto

TARGETS = build/client build/client2
SOURCES = client.c client2.c

all: build $(TARGETS)

build:
	mkdir -p build

build/client: client.c
	$(CC) $(CFLAGS) -o build/client client.c $(LDFLAGS)

build/client2: client2.c
	$(CC) $(CFLAGS) -o build/client2 client2.c $(LDFLAGS)

clean:
	rm -rf build
