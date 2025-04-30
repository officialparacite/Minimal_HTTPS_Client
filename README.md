# HTTPS Client in C using OpenSSL

This repository contains a simple HTTPS client written in C using OpenSSL. The program demonstrates how to make secure HTTP requests over SSL/TLS using the OpenSSL library.

## Files

### `client.c`
- A basic HTTPS client.
- Connects to an HTTPS server using SSL/TLS.
- Sends a GET request and prints the response.
- dynamically allocates memory for the response.

## Build Instructions

Make sure you have OpenSSL development libraries installed.

To build the client, run:

```bash
make
```

To clean up:

```bash
make clean
```
