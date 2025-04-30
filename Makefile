CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lssl -lcrypto

TARGET = build/client
SRC = client.c

all: build $(TARGET)

build:
	mkdir -p build

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -rf build
