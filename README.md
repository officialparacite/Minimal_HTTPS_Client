# HTTPS Client in C using OpenSSL

This repository contains two simple HTTPS clients written in C using OpenSSL. These programs demonstrate how to make secure HTTP requests over SSL/TLS using the OpenSSL library.

## Files

### `client.c`
- A basic HTTPS client.
- Connects to an HTTPS server using SSL/TLS.
- Sends a GET request and prints the response.
- Uses static buffers for storing the response.

### `client2.c`
- Same as `client.c`, but dynamically allocates memory using `malloc()` to store the response.
- Useful for handling larger or unknown response sizes.

## Build Instructions

Make sure you have OpenSSL development libraries installed.

To build the clients, run:

```bash
make
```

To clean up:

```bash
make clean
```
