# Guide to HTTPS/TLS Client Implementation

This guide walks through creating a secure HTTPS client using OpenSSL, explaining each step of the TLS client connection flow with detailed code examples and explanations.

## Table of Contents
1. [Introduction to HTTPS/TLS](#introduction-to-httpstls)
2. [Prerequisites](#prerequisites)
3. [DNS Resolution with getaddrinfo()](#dns-resolution-with-getaddrinfo)
4. [Creating a Socket](#creating-a-socket)
5. [TCP Connection](#tcp-connection)
6. [OpenSSL Initialization](#openssl-initialization)
7. [Creating an SSL Context](#creating-an-ssl-context)
8. [Setting Security Options](#setting-security-options)
9. [Certificate Verification Setup](#certificate-verification-setup)
10. [Creating an SSL Object](#creating-an-ssl-object)
11. [Server Name Indication (SNI)](#server-name-indication-sni)
12. [Associating Socket with SSL](#associating-socket-with-ssl)
13. [Performing the TLS Handshake](#performing-the-tls-handshake)
14. [Verifying the Certificate](#verifying-the-certificate)
15. [Secure Communication](#secure-communication)
16. [Graceful Shutdown](#graceful-shutdown)
17. [Error Handling](#error-handling)
18. [Complete Example](#complete-example)
19. [Common Issues and Debugging](#common-issues-and-debugging)
20. [Advanced Topics](#advanced-topics)

## Introduction to HTTPS/TLS

HTTPS is HTTP over a secure connection using TLS (Transport Layer Security, the successor to SSL). It provides:

- **Confidentiality**: Communication is encrypted
- **Authentication**: The server's identity is verified
- **Integrity**: Data cannot be tampered with undetected

This guide explains how to implement a client that can establish secure HTTPS connections. We'll use OpenSSL, the most widely used open-source TLS library.

## Prerequisites

You'll need:

- A C compiler (gcc, clang, etc.)
- OpenSSL development libraries
- Basic understanding of socket programming

Install OpenSSL development package:

**Ubuntu/Debian**:
```bash
sudo apt-get install libssl-dev
```

**Fedora/RHEL**:
```bash
sudo dnf install openssl-devel
```

**macOS** (using Homebrew):
```bash
brew install openssl
```

When compiling, link against OpenSSL:
```bash
gcc -o https_client https_client.c -lssl -lcrypto
```

Include the necessary headers in your program:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
```

## DNS Resolution with getaddrinfo()

The first step in establishing a connection is resolving the domain name to an IP address with `getaddrinfo()`. This function allows you to perform DNS lookups in a protocol-independent way.

```c
int dns_lookup(const char *hostname, const char *port, struct addrinfo **result) {
    struct addrinfo hints;
    int status;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    
    // Resolve the domain name
    status = getaddrinfo(hostname, port, &hints, result);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        return -1;
    }
    
    return 0;
}
```

This function:
1. Sets up `hints` to specify we want a TCP socket (SOCK_STREAM)
2. Uses AF_UNSPEC to allow both IPv4 and IPv6
3. Populates the `result` with a linked list of address structures

The caller must free the result with `freeaddrinfo()` when done.

## Creating a Socket

Once we have the address information, we create a socket using the `socket()` function:

```c
int create_socket(struct addrinfo *addr_list) {
    struct addrinfo *current;
    int sockfd;
    
    // Try each address until we successfully connect
    for (current = addr_list; current != NULL; current = current->ai_next) {
        // Create socket
        sockfd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (sockfd < 0) {
            continue; // Try next address
        }
        
        // If we got a valid socket, we'll use this address
        return sockfd;
    }
    
    // If we reach here, we failed to create a socket
    return -1;
}
```

This function:
1. Iterates through the address list returned by `getaddrinfo()`
2. Creates a socket for each address until it gets a valid socket descriptor
3. Returns the valid socket descriptor or -1 if all attempts fail

## TCP Connection

After creating a socket, we need to establish a TCP connection to the server:

```c
int establish_connection(int sockfd, struct addrinfo *addr_list) {
    struct addrinfo *current;
    
    // Try each address until we successfully connect
    for (current = addr_list; current != NULL; current = current->ai_next) {
        // Try to connect
        if (connect(sockfd, current->ai_addr, current->ai_addrlen) == 0) {
            return 0; // Connection successful
        }
        
        // Connection failed, try next address
    }
    
    return -1; // All connection attempts failed
}
```

The `connect()` function:
1. Takes the socket descriptor
2. Takes a pointer to the address structure and its length
3. Returns 0 on success, -1 on failure

This function attempts to connect to each address in the list until it succeeds or exhausts all options.

## OpenSSL Initialization

Before using OpenSSL, you need to initialize the library. In older versions, you had to explicitly initialize the library with `SSL_library_init()` and `OpenSSL_add_all_algorithms()`. In newer versions (OpenSSL 1.1.0+), the library auto-initializes, but it's still good practice to handle both cases:

```c
void init_openssl() {
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
    #else
        // OpenSSL 1.1.0+ auto-initializes
    #endif
}

void cleanup_openssl() {
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
        EVP_cleanup();
        ERR_free_strings();
    #else
        // OpenSSL 1.1.0+ auto-cleans up
    #endif
}
```

## Creating an SSL Context

The SSL context (`SSL_CTX`) is the foundation for all SSL operations. It holds configuration, certificate information, and other settings:

```c
SSL_CTX *create_ssl_context() {
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    
    // Choose the SSL/TLS protocol version
    method = TLS_client_method(); // Negotiate highest available SSL/TLS version
    if (!method) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    
    // Create a new SSL context
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    
    return ctx;
}
```

This function:
1. Selects the TLS client method (which allows negotiation of the highest mutually supported protocol version)
2. Creates a new context with that method
3. Returns the context or NULL on failure

## Setting Security Options

Next, we'll set security options for our SSL context to enforce modern security practices:

```c
int configure_context(SSL_CTX *ctx) {
    // Set minimum TLS protocol version (TLS 1.2)
    if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION)) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    // Set security options
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | 
                             SSL_OP_NO_TLSv1_1 | SSL_OP_NO_COMPRESSION);
    
    // Use system certificate store
    if (!SSL_CTX_set_default_verify_paths(ctx)) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    // Set verification mode
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    
    // Implement modern cipher preferences
    if (!SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!kRSA:!PSK:!SRP:!MD5:!RC4")) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    return 0;
}
```

This function:
1. Sets the minimum TLS version to 1.2 (considered secure as of 2023)
2. Disables insecure SSL/TLS versions and compression
3. Loads the system's CA certificates for verification
4. Enables peer verification (checking server certificates)
5. Sets a secure cipher list

### Understanding SSL_CTX_set_options()

Here's what each option means:
- `SSL_OP_NO_SSLv2`: Disable SSL v2 (highly insecure)
- `SSL_OP_NO_SSLv3`: Disable SSL v3 (vulnerable to POODLE attack)
- `SSL_OP_NO_TLSv1`: Disable TLS 1.0 (vulnerable to BEAST attack)
- `SSL_OP_NO_TLSv1_1`: Disable TLS 1.1 (has vulnerabilities)
- `SSL_OP_NO_COMPRESSION`: Disable compression (vulnerable to CRIME attack)

## Certificate Verification Setup

Certificate verification is crucial for HTTPS security. It ensures you're talking to the legitimate server:

```c
void setup_certificate_verification(SSL_CTX *ctx) {
    // Already loaded system certificates with SSL_CTX_set_default_verify_paths()
    
    // Optional: Set verification depth
    SSL_CTX_set_verify_depth(ctx, 5);
    
    // Optional: Set a callback for custom verification logic
    // SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verify_callback);
}

// Optional custom verification callback
int verify_callback(int preverify_ok, X509_STORE_CTX *x509_ctx) {
    // This is called during certificate verification
    // Return 1 to accept the certificate or 0 to reject
    if (!preverify_ok) {
        int err = X509_STORE_CTX_get_error(x509_ctx);
        fprintf(stderr, "Certificate verification error: %s\n", 
                X509_verify_cert_error_string(err));
        return 0; // Reject
    }
    
    return 1; // Accept
}
```

The verification process:
1. When you set `SSL_VERIFY_PEER`, OpenSSL verifies the server's certificate chain
2. It checks the certificate's validity, signature, and that it's trusted
3. It also checks that the certificate matches the hostname (when properly configured)

## Creating an SSL Object

With the context configured, we create an SSL object that represents a specific connection:

```c
SSL *create_ssl_connection(SSL_CTX *ctx, int sockfd) {
    SSL *ssl;
    
    // Create new SSL structure
    ssl = SSL_new(ctx);
    if (!ssl) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    
    return ssl;
}
```

The SSL object is tied to a specific connection and contains the session state.

## Server Name Indication (SNI)

SNI is crucial for virtual hosting environments where multiple HTTPS websites share a single IP address:

```c
int set_server_name(SSL *ssl, const char *hostname) {
    // Set SNI (Server Name Indication)
    if (!SSL_set_tlsext_host_name(ssl, hostname)) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    // Set hostname for certificate verification
    if (!SSL_set1_host(ssl, hostname)) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    return 0;
}
```

This function:
1. Sets the SNI extension with the server's hostname (so the server knows which certificate to present)
2. Sets the hostname for certificate verification (makes sure the certificate matches the hostname)

## Associating Socket with SSL

Before we can perform the TLS handshake, we need to associate our socket with the SSL object:

```c
int associate_socket(SSL *ssl, int sockfd) {
    // Attach the socket to the SSL object
    if (!SSL_set_fd(ssl, sockfd)) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    return 0;
}
```

This function simply binds the socket file descriptor to the SSL object.

## Performing the TLS Handshake

Now we're ready to perform the TLS handshake, which establishes the secure connection:

```c
int perform_handshake(SSL *ssl) {
    int ret;
    
    // Perform the TLS handshake
    ret = SSL_connect(ssl);
    if (ret <= 0) {
        int err = SSL_get_error(ssl, ret);
        fprintf(stderr, "SSL handshake failed with error code: %d\n", err);
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    // Print the negotiated cipher
    printf("SSL connection established using %s\n", SSL_get_cipher(ssl));
    
    return 0;
}
```

During the handshake:
1. The client and server negotiate the TLS protocol version and cipher suite
2. The server sends its certificate
3. The client verifies the certificate
4. They exchange keys and establish the session

## Verifying the Certificate

After the handshake, we should verify that certificate validation succeeded:

```c
int verify_certificate(SSL *ssl) {
    X509 *cert;
    long verify_result;
    
    // Check the verification result
    verify_result = SSL_get_verify_result(ssl);
    if (verify_result != X509_V_OK) {
        fprintf(stderr, "Certificate verification failed: %s\n", 
                X509_verify_cert_error_string(verify_result));
        return -1;
    }
    
    // Print certificate information (optional)
    cert = SSL_get_peer_certificate(ssl);
    if (cert) {
        char *subject = X509_NAME_oneline(X509_get_subject_name(cert), NULL, 0);
        char *issuer = X509_NAME_oneline(X509_get_issuer_name(cert), NULL, 0);
        
        printf("Server certificate:\n");
        printf("  Subject: %s\n", subject);
        printf("  Issuer: %s\n", issuer);
        
        free(subject);
        free(issuer);
        X509_free(cert);
    } else {
        fprintf(stderr, "No certificate presented by the server\n");
        return -1;
    }
    
    return 0;
}
```

This function:
1. Gets the result of the certificate verification
2. Checks that it's `X509_V_OK` (valid)
3. Optionally retrieves and displays certificate information

## Secure Communication

With the secure connection established, we can now send and receive data:

```c
int send_request(SSL *ssl, const char *request) {
    int bytes;
    
    // Write data to the SSL connection
    bytes = SSL_write(ssl, request, strlen(request));
    if (bytes <= 0) {
        int err = SSL_get_error(ssl, bytes);
        fprintf(stderr, "SSL_write failed with error: %d\n", err);
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    return bytes;
}

int receive_response(SSL *ssl, char *buffer, size_t buffer_size) {
    int bytes;
    
    // Read data from the SSL connection
    bytes = SSL_read(ssl, buffer, buffer_size - 1);
    if (bytes <= 0) {
        int err = SSL_get_error(ssl, bytes);
        fprintf(stderr, "SSL_read failed with error: %d\n", err);
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    // Null-terminate the buffer
    buffer[bytes] = '\0';
    
    return bytes;
}
```

`SSL_write()` and `SSL_read()` work similarly to `write()` and `read()` but handle the encryption and decryption automatically.

## Graceful Shutdown

When we're done, we perform a clean shutdown to prevent truncation attacks:

```c
void cleanup_connection(SSL *ssl, SSL_CTX *ctx, int sockfd) {
    // Bidirectional SSL shutdown
    SSL_shutdown(ssl);
    
    // Free SSL object
    SSL_free(ssl);
    
    // Free SSL context
    SSL_CTX_free(ctx);
    
    // Close socket
    close(sockfd);
}
```

`SSL_shutdown()` performs a bidirectional TLS shutdown, ensuring both sides acknowledge the connection is ending.

## Error Handling

OpenSSL errors should be handled properly. Here's a function to print OpenSSL errors:

```c
void print_openssl_errors() {
    unsigned long err;
    const char *file, *data;
    int line, flags;
    
    while ((err = ERR_get_error_line_data(&file, &line, &data, &flags)) != 0) {
        char errbuf[256];
        ERR_error_string_n(err, errbuf, sizeof(errbuf));
        fprintf(stderr, "OpenSSL error: %s:%d: %s\n", file, line, errbuf);
        if (data && (flags & ERR_TXT_STRING)) {
            fprintf(stderr, "Additional data: %s\n", data);
        }
    }
}
```

Use this function whenever an OpenSSL function fails to get detailed error information.

## Complete Example

Here's a complete example putting everything together:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

// Initialize OpenSSL
void init_openssl() {
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
    #endif
}

// Clean up OpenSSL
void cleanup_openssl() {
    #if OPENSSL_VERSION_NUMBER < 0x10100000L
        EVP_cleanup();
        ERR_free_strings();
    #endif
}

// Create an SSL context
SSL_CTX *create_ssl_context() {
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    
    method = TLS_client_method();
    if (!method) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    
    ctx = SSL_CTX_new(method);
    if (!ctx) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }
    
    return ctx;
}

// Configure SSL context
int configure_context(SSL_CTX *ctx) {
    // Set minimum TLS version to 1.2
    if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION)) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    // Disable old/insecure protocols and compression
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | 
                           SSL_OP_NO_TLSv1_1 | SSL_OP_NO_COMPRESSION);
    
    // Load system CA certificates
    if (!SSL_CTX_set_default_verify_paths(ctx)) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    // Verify server certificates
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    
    // Set verification depth
    SSL_CTX_set_verify_depth(ctx, 5);
    
    return 0;
}

// Main function
int main(int argc, char *argv[]) {
    const char *hostname = "example.com";
    const char *port = "443";
    struct addrinfo hints, *res;
    int sockfd;
    SSL_CTX *ctx;
    SSL *ssl;
    char request[1024];
    char response[4096];
    int bytes;
    
    // Check command line arguments
    if (argc > 1) {
        hostname = argv[1];
    }
    
    // Initialize OpenSSL
    init_openssl();
    
    // Create SSL context
    ctx = create_ssl_context();
    if (!ctx) {
        fprintf(stderr, "Failed to create SSL context\n");
        exit(EXIT_FAILURE);
    }
    
    // Configure SSL context
    if (configure_context(ctx) != 0) {
        fprintf(stderr, "Failed to configure SSL context\n");
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }
    
    // Set up server address structure
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    // Resolve hostname
    if (getaddrinfo(hostname, port, &hints, &res) != 0) {
        fprintf(stderr, "Failed to resolve hostname\n");
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }
    
    // Create socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to create socket\n");
        freeaddrinfo(res);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }
    
    // Connect to server
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
        fprintf(stderr, "Failed to connect\n");
        close(sockfd);
        freeaddrinfo(res);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }
    
    freeaddrinfo(res);
    
    // Create SSL object
    ssl = SSL_new(ctx);
    if (!ssl) {
        fprintf(stderr, "Failed to create SSL object\n");
        close(sockfd);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }
    
    // Set SNI hostname
    if (!SSL_set_tlsext_host_name(ssl, hostname)) {
        fprintf(stderr, "Failed to set SNI hostname\n");
        SSL_free(ssl);
        close(sockfd);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }
    
    // Set hostname for verification
    if (!SSL_set1_host(ssl, hostname)) {
        fprintf(stderr, "Failed to set verification hostname\n");
        SSL_free(ssl);
        close(sockfd);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }
    
    // Attach socket to SSL object
    if (!SSL_set_fd(ssl, sockfd)) {
        fprintf(stderr, "Failed to set SSL file descriptor\n");
        SSL_free(ssl);
        close(sockfd);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }
    
    // Perform TLS handshake
    if (SSL_connect(ssl) <= 0) {
        fprintf(stderr, "SSL handshake failed\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(sockfd);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }
    
    // Verify server certificate
    long verify_result = SSL_get_verify_result(ssl);
    if (verify_result != X509_V_OK) {
        fprintf(stderr, "Certificate verification failed: %s\n", 
                X509_verify_cert_error_string(verify_result));
        SSL_free(ssl);
        close(sockfd);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }
    
    // Display connection info
    printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
    
    // Prepare HTTP request
    snprintf(request, sizeof(request), 
             "GET / HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: OpenSSL-HTTPS-Client\r\n"
             "Connection: close\r\n"
             "\r\n",
             hostname);
    
    // Send HTTP request
    if (SSL_write(ssl, request, strlen(request)) <= 0) {
        fprintf(stderr, "Failed to send request\n");
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(sockfd);
        SSL_CTX_free(ctx);
        exit(EXIT_FAILURE);
    }
    
    // Receive HTTP response
    bytes = SSL_read(ssl, response, sizeof(response) - 1);
    if (bytes <= 0) {
        fprintf(stderr, "Failed to receive response\n");
        ERR_print_errors_fp(stderr);
    } else {
        response[bytes] = '\0';
        printf("Received %d bytes:\n%s\n", bytes, response);
    }
    
    // Clean up
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sockfd);
    SSL_CTX_free(ctx);
    cleanup_openssl();
    
    return 0;
}
```

## Common Issues and Debugging

### Handling Non-blocking I/O

The above example assumes blocking I/O. For non-blocking sockets, handle `SSL_ERROR_WANT_READ` and `SSL_ERROR_WANT_WRITE`:

```c
int ssl_read_nonblocking(SSL *ssl, char *buffer, int buffer_size) {
    int bytes, error;
    
    do {
        bytes = SSL_read(ssl, buffer, buffer_size - 1);
        if (bytes <= 0) {
            error = SSL_get_error(ssl, bytes);
            if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
                // Would block, try again later
                return 0;
            } else {
                // Real error
                ERR_print_errors_fp(stderr);
                return -1;
            }
        }
    } while (bytes <= 0);
    
    buffer[bytes] = '\0';
    return bytes;
}
```

### Certificate Verification Failures

Common causes of certificate verification failures:
- Server certificate is self-signed or not issued by a trusted CA
- Certificate has expired
- Certificate doesn't match the hostname
- Intermediate certificates are missing from the server's chain

To debug these, you can use `openssl s_client`:

```bash
openssl s_client -connect example.com:443 -servername example.com
```

### SSL_connect Fails

If `SSL_connect()` fails, check:
- Network connectivity
- Server support for your TLS version
- Cipher compatibility
- Proxy or firewall interference

### Handling Timeouts

Add socket-level timeouts:

```c
struct timeval timeout;
timeout.tv_sec = 10;
timeout.tv_usec = 0;

if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("setsockopt failed");
}

if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
    perror("setsockopt failed");
}
```

## Advanced Topics

### Certificate Pinning

Certificate pinning increases security by verifying that the server's certificate matches a known "pinned" certificate:

```c
int pin_certificate(SSL *ssl, const unsigned char *expected_hash, size_t hash_len) {
    X509 *cert;
    unsigned char cert_hash[EVP_MAX_MD_SIZE];
    unsigned int hash_size;
    
    cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        return -1;
    }
    
    // Calculate SHA-256 hash of the certificate
    if (!X509_digest(cert, EVP_sha256(), cert_hash, &hash_size)) {
        X509_free(cert);
        return -1;
    }
    
    X509_free(cert);
    
    // Compare with expected hash
    if (hash_size != hash_len || memcmp(cert_hash, expected_hash, hash_size) != 0) {
        return -1;
    }
    
    return 0;
}
```

### Client Certificates

For mutual TLS (mTLS), where the client also authenticates with a certificate:

```c
int configure_client_certificate(SSL_CTX *ctx, const char *cert_file, const char *key_file) {
    // Load client certificate
    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    // Load private key
    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    
    // Verify private key matches certificate
    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match certificate\n");
        return -1;
    }
    
    return 0;
}
```

---
