
---

HTTPS/TLS Client Connection Flow:

- getaddrinfo()                       // DNS resolution
- socket()                            // Create socket
- connect()                           // TCP connect
- SSL_CTX_new()                       // Create SSL context
- SSL_CTX_set_min_proto_version()     // Set minimum TLS version (TLS 1.2+)
- SSL_CTX_set_options()               // Set security options
- SSL_CTX_set_default_verify_paths()  // Load system CA certs
- SSL_CTX_set_verify()                // Enable certificate verification
- SSL_new()                           // Create SSL object
- SSL_set_tlsext_host_name()          // Set SNI (Server Name Indication)
- SSL_set1_host()                     // Set hostname for verification (modern)
- SSL_set_fd()                        // Attach socket to SSL object
- SSL_connect()                       // TLS handshake
- SSL_get_verify_result()             // Check certificate verification result
- SSL_write() / SSL_read()            // Encrypted communication
- SSL_shutdown()                      // Clean shutdown
- SSL_free()                          // Free SSL object
- SSL_CTX_free()                      // Free context
- close()                             // Close socket

---

# What is OpenSSL?

**OpenSSL** is like a **big toolbox**.

Inside the toolbox:
- Tools for making **secure internet connections** (SSL/TLS).
- Tools for **encrypting/decrypting data**.
- Tools for **hashing** (creating checksums, signatures).
- Tools for **handling certificates**.
- Tools for **generating random numbers**.

Note: **SSL/TLS** is just **one** set of tools inside this toolbox.

---

# What is `SSL_library_init()`?

When you call `SSL_library_init()`, you are saying:

> "Get all the **tools needed for SSL/TLS** ready."

At program start:
- OpenSSL **does not know** what encryption methods you want.
- OpenSSL **has not loaded** the list of ciphers and hashes.
- OpenSSL **has not set up** the memory needed for SSL connections.

`SSL_library_init()` prepares everything inside OpenSSL needed to **make secure SSL/TLS connections**.

---

# What Gets Loaded?

## SSL Algorithms

"SSL algorithms" are just **mathematical formulas** used to secure data.

Examples:
- Encrypt messages (AES, ChaCha20).
- Create digital signatures (RSA, ECDSA).
- Verify data integrity (SHA256).

SSL/TLS needs **encryption**, **hashing**, and **key exchange** — all of these use algorithms.

---

## Ciphers and Digests

### What is a Cipher?

- A **cipher** is a **method to encrypt and decrypt data**.
- It scrambles information so unauthorized users cannot read it.

When you send secret data (like a password) across the internet, you **encrypt** it using a **cipher**.

Examples of ciphers:

| Cipher | Purpose |
|:---|:---|
| AES | Strong, fast encryption (used everywhere today) |
| ChaCha20 | Strong encryption, optimized for mobile devices |
| 3DES | Outdated and weak encryption (not recommended) |

---

### What is a Digest?

- A **digest** is a **summary (hash)** of data.
- It’s **one-way**: you can create it, but you **cannot reverse** it.
- It is used to **check if data has been tampered with**.

Examples of digests:

| Digest (Hash Function) | Purpose |
|:---|:---|
| SHA256 | Strong 256-bit fingerprint |
| SHA512 | Longer, even stronger hash |
| MD5 | Outdated and insecure (avoid using) |

---

# Quick Example

Suppose you want to send a **secret message**:

| Component | Example |
|:---|:---|
| Cipher | Use AES to encrypt the message |
| Digest | Use SHA256 to create a fingerprint of the message |

---

# In SSL/TLS

- A **cipher** encrypts the data so nobody can see your communication.
- A **digest** ensures the data wasn’t tampered with.

Both work **together** to keep your connection **confidential** and **integrity-protected**.

Thus, **OpenSSL must load both ciphers and digests** — because SSL/TLS **depends on both**.

---

# Preparing Internal Data Structures

Before OpenSSL can handle secure communications, it must set up several internal data structures.

These include:
- Tracking active SSL connections.
- Listing supported protocols (TLS 1.2, TLS 1.3, etc.).
- Managing session resumption for faster reconnects.
- Handling internal error reporting and diagnostics.

Calling `SSL_library_init()` performs all this background setup.  
Without it, SSL operations may **crash**, **fail silently**, or behave **unpredictably**.

Proper initialization ensures OpenSSL can operate **reliably** and **securely**.

## Note: In OpenSSL 1.1.0 and later, SSL_library_init() is no longer required, as initialization is handled automatically during the first use of OpenSSL functions. However, it is safe to include OpenSSL headers in your code regardless of the OpenSSL version, as long as you are using the correct version for your system. OpenSSL's headers are designed to be included in your source files, and they do not cause any issues by simply being included, even in the case where manual initialization is not required (OpenSSL 1.1.0 and later).

---

---

# what is SSL_load_error_strings()?

`SSL_load_error_strings` is a function used to load error strings related to SSL and TLS errors into OpenSSL’s error string system.

Here’s a brief explanation of what it does:

- **`SSL_load_error_strings`**: It populates OpenSSL’s internal error string system, and you need to manually retrieve errors from the OpenSSL error queue using functions like `ERR_get_error()` or `ERR_error_string()`. This is more explicit and specific to OpenSSL’s error handling system.

It is usually called at the beginning of a program that makes use of SSL/TLS functions to ensure that detailed error messages are available when needed.

### Example Usage
```c
#include <openssl/ssl.h>
#include <openssl/err.h>

int main() {
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings(); // Load error strings for SSL-related errors

    // Your SSL code here

    // Clean up
    ERR_free_strings();
    return 0;
}
```

By calling `SSL_load_error_strings`, any SSL-related error codes encountered can be translated into readable strings, making debugging and error handling easier.

Note that `SSL_library_init` is often used alongside `SSL_load_error_strings` in OpenSSL applications to initialize the SSL/TLS library and error strings.

## Note: In OpenSSL 1.1.0 and later, SSL_load_error_strings() is no longer required, as initialization is handled automatically during the first use of OpenSSL functions. However, it is safe to include OpenSSL headers in your code regardless of the OpenSSL version, as long as you are using the correct version for your system. OpenSSL's headers are designed to be included in your source files, and they do not cause any issues by simply being included, even in the case where manual initialization is not required (OpenSSL 1.1.0 and later).

---

---

# **creating an SSL connection** requires **two functions**.

---

### 1. `SSL_CTX_new()`

This function **creates the context** (the configuration) that defines how SSL/TLS should behave for your program. It sets things like:

- Which version of TLS/SSL to use (e.g., TLS 1.2, TLS 1.3).
- The certificates you’ll trust.
- The ciphers you’ll allow.

So, **this is the global configuration** you'll use for **one or many connections**.

### 2. `SSL_new(ctx)`

Once you have a context (`ctx`), you **create an actual connection** with `SSL_new()`. This function:

- Creates an `SSL` object.
- **Uses** the settings from the context (`ctx`) you pass in.

This object represents **one connection** to a server (or client) and contains everything needed to establish an SSL/TLS session.

---

### Why Two Functions?

- **First, create the context (`SSL_CTX_new`)** — this is your "blueprint."
- **Then, create the connection (`SSL_new`)** — this is the actual connection that will use that blueprint.

Let's break it down:

- **`SSL_CTX_new()`** takes a **function pointer** as an argument (like `TLS_client_method()`).
- That function pointer **tells OpenSSL what kind of SSL/TLS method you want to use** (e.g., client-side or server-side).

### What does `TLS_client_method()` do?

- **`TLS_client_method()`** is a function that **returns a pointer** to a specific `SSL_METHOD` structure, which defines how the client-side connection will behave.
- This structure contains all the details for the handshake process and SSL/TLS settings that are specific to **client-side communication**.

So, **`TLS_client_method()`** is just returning a pointer to that structure. When you pass this pointer to `SSL_CTX_new()`, OpenSSL uses it to configure the context (`SSL_CTX`) for the SSL connection.

---

### Code Example:

```c
// 1. Create SSL context (settings for TLS connections)
SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());

// 2. Set context to verify server certs
SSL_CTX_set_default_verify_paths(ctx);

// 3. Create a new SSL connection object using the context
SSL *ssl = SSL_new(ctx);

```

### if OpenSSL can't find certificates do this:

```c
// Specify the certificate file explicitly
SSL_CTX_load_verify_locations(ctx, "/etc/ssl/certs/ca-certificates.crt", NULL);

// OR specify a directory of certificates
SSL_CTX_load_verify_locations(ctx, NULL, "/etc/ssl/certs/");
```
---

### I know, I know — I said it only takes two functions, but hear me out.

---

## **What is TLS (or HTTPS) doing?**

When your C program connects to a site like `https://example.com`, it does 2 things:

1. **Encrypts** the connection — so nobody can read the data you're sending/receiving.
2. **Authenticates** the server — to make sure you're really talking to *example.com*, not an attacker pretending to be it.

---

## Why is server authentication important?

Imagine someone creates a fake `example.com` server and tricks you (via DNS poisoning, public Wi-Fi, etc.) into connecting to it. Without authentication, **your program will happily connect to the fake server**, and send data (like passwords) to the attacker.

To prevent this, the real server presents an **SSL certificate**. This certificate says:
> *"I am example.com, and a trusted Certificate Authority (like Let's Encrypt or DigiCert) vouches for me."*

---

## Certificate Authorities (CAs)

These are third-party companies you trust (built into your OS). They digitally **sign** certificates for real websites.

Your system has a list of CA public keys, usually stored in:
- `/etc/ssl/certs/`
- `/etc/pki/tls/certs/`

These are used to **verify** that the server’s certificate is legitimate.

---

### `SSL_CTX_set_default_verify_paths(ctx);`
Tells OpenSSL:
> *"Use the system’s built-in list of trusted certificate authorities to verify the server."*

Without this, OpenSSL doesn't know **who to trust**, and **certificate verification fails** — even for real websites.

---

---

### Hostname Verification

1. **Setting SNI (Server Name Indication)**:
   ```c
   SSL_set_tlsext_host_name(ssl, hostname);
   ```
   This tells the server which hostname you're trying to connect to, so it can present the correct certificate if it hosts multiple domains.

2. **Setting the hostname for verification**:
   ```c
   SSL_set1_host(ssl, hostname);
   ```
   This is the crucial step that tells OpenSSL to verify that the certificate presented by the server is valid for the hostname you're connecting to.

3. **Checking the verification result**:
   ```c
   long verify_result = SSL_get_verify_result(ssl);
   if (verify_result == X509_V_OK) {
       // Certificate verification passed
   }
   ```
   This checks that the certificate is valid, trusted, and matches the hostname you specified with `SSL_set1_host()`.

In older versions of OpenSSL (before 1.0.2), hostname verification was not built into the library and had to be done manually by examining the certificate's Common Name (CN) and Subject Alternative Names (SAN) fields after the handshake.

---

---

# Wrap the socket with the TLS/SSL protocol

`SSL_set_fd()` is the function used to associate the SSL object with a file descriptor (like a socket) for secure communication.

Here’s how it works:

- It "wraps" an SSL/TLS layer around a socket or file descriptor that you’ve already created. This allows you to use SSL/TLS protocols to securely send and receive data over that socket.
- After calling `SSL_set_fd()`, you can perform an SSL handshake (via `SSL_connect()` for clients or `SSL_accept()` for servers) to establish a secure communication channel.

---

---

# Upgrade the connection to TLS/SSL

`SSL_connect()` is an OpenSSL function that is used to initiate the **SSL/TLS handshake** for a **client** that has already established a regular (insecure) connection over a socket.

Here’s why it’s needed even after the socket is already connected:

### Why Do You Need `SSL_connect()` After `connect()`?
- **`connect()`**: This system call is used to establish a basic, unencrypted connection between a client and a server (for example, using TCP). At this stage, you're just setting up a regular communication channel without any encryption.

- **`SSL_connect()`**: Once you've established the initial connection with `connect()`, **`SSL_connect()`** is needed to upgrade the connection to an SSL/TLS-secured one. It initiates the **SSL/TLS handshake** process, where the client and server agree on encryption algorithms, exchange certificates (if needed), and establish a secure channel. The purpose of `SSL_connect()` is to initiate this handshake and transition the connection into a secure one.

### Why Do We Need to "Connect" Again?
- The `connect()` system call only sets up the raw, insecure connection (using TCP/IP, for example). SSL/TLS protocols work at a higher layer and need to perform their own handshake, which involves encryption setup, key exchange, certificate verification, and other cryptographic operations. This is why **`SSL_connect()`** is required **after** the initial connection is made with `connect()`.
  
- `SSL_connect()` essentially turns the already-connected socket into a **secure channel** by adding the SSL/TLS layer on top of the existing connection. The raw socket connection (via `connect()`) becomes encrypted and secure after `SSL_connect()` completes the handshake.

---

---

# Sending data over the newly created SSL/TLS connection

`SSL_write()` is a function in the OpenSSL library that is used to send data over an SSL/TLS connection. It works similarly to regular socket-based write functions but ensures that the data is transmitted securely using SSL/TLS encryption.

### Function Signature:
```c
int SSL_write(SSL *ssl, const void *buf, int num);
```

### Parameters:
- **`ssl`**: The SSL object associated with the connection (which was previously established using `SSL_connect()` for a client or `SSL_accept()` for a server). This object manages the SSL/TLS session.
- **`buf`**: A pointer to the buffer containing the data you want to send.
- **`num`**: The number of bytes from the buffer `buf` to send.

### Return Value:
- **On success**: The number of bytes actually written. This could be less than the number of bytes requested (in which case you should call `SSL_write()` again to send the remaining data).
- **On failure**: A negative value. You can use `SSL_get_error()` to get more details about the error.

### How It Works:
1. **Encryption**: `SSL_write()` encrypts the data before sending it over the network. The encryption is done based on the negotiated SSL/TLS cipher suite during the handshake (which happens via `SSL_connect()` or `SSL_accept()`).
   
2. **Buffered Writing**: The data you send with `SSL_write()` is buffered and might not be sent immediately. It will be written to the underlying transport layer (usually the socket) after proper SSL/TLS processing.

3. **SSL/TLS Layer**: The function ensures the data is securely transmitted. It handles things like:
   - Encrypting the data.
   - Adding any necessary SSL/TLS protocol overhead (such as padding, headers, etc.).
   - Managing retransmissions if needed (for example, if part of the data fails to be transmitted).

4. **Under the Hood**: Internally, `SSL_write()` calls the underlying `write()` function (or equivalent), but instead of sending plain data, it sends the encrypted data and also handles SSL/TLS-specific operations like fragmentation.

---

---

# Receiving data over the newly created SSL/TLS connection

`SSL_read()` is an OpenSSL function used to **receive data from an SSL/TLS connection**. It is the counterpart to `SSL_write()`, but instead of sending data over the encrypted channel, it reads the encrypted data and decrypts it before providing it to the user.

### Function Signature:
```c
int SSL_read(SSL *ssl, void *buf, int num);
```

### Parameters:
- **`ssl`**: The SSL object associated with the connection. This SSL object represents the secure connection that was established via `SSL_connect()` (client) or `SSL_accept()` (server).
- **`buf`**: A pointer to a buffer where the decrypted data will be stored.
- **`num`**: The maximum number of bytes to read from the connection (this is the size of the buffer).

### Return Value:
- **On success**: The number of bytes actually read and decrypted.
- **On error**: A negative value, and you can use `SSL_get_error()` to get more details on the error.

### How It Works:
- `SSL_read()` reads encrypted data from the underlying socket, decrypts it, and copies the decrypted data into the provided buffer (`buf`).
- Internally, OpenSSL handles the details of decrypting the data, verifying SSL/TLS integrity, handling reassembly (if data was fragmented), and managing any necessary retransmissions.
- The returned number of bytes may be less than `num` because of how SSL/TLS protocols handle data fragmentation or flow control.

### Example Usage:
Here’s a simple example where you use `SSL_read()` to receive data from an SSL/TLS connection:

```c
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <stdio.h>

int main() {
    SSL_CTX *ctx;
    SSL *ssl;
    int server_fd = 0;  // Assume this is a valid socket FD

    // Initialize OpenSSL
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    const SSL_METHOD *method = TLS_client_method();
    ctx = SSL_CTX_new(method);
    
    if (ctx == NULL) {
        fprintf(stderr, "Unable to create SSL context\n");
        return 1;
    }

    // Create an SSL object
    ssl = SSL_new(ctx);
    
    if (ssl == NULL) {
        fprintf(stderr, "Unable to create SSL object\n");
        return 1;
    }

    // Associate the socket with SSL
    if (SSL_set_fd(ssl, server_fd) == 0) {
        fprintf(stderr, "Unable to associate the socket with SSL\n");
        return 1;
    }

    // Perform the SSL handshake
    if (SSL_connect(ssl) != 1) {
        fprintf(stderr, "SSL handshake failed\n");
        return 1;
    }

    // Receive encrypted data
    char buffer[1024];
    int bytes_read = SSL_read(ssl, buffer, sizeof(buffer));

    if (bytes_read < 0) {
        fprintf(stderr, "SSL_read failed\n");
        return 1;
    }

    // Print the received data
    printf("Received %d bytes: %s\n", bytes_read, buffer);

    // Cleanup
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    return 0;
}
```

### Key Concepts:
- **Decryption**: `SSL_read()` decrypts the data automatically using the SSL/TLS keys and protocols established during the handshake.
- **Secure Communication**: Like `SSL_write()`, `SSL_read()` ensures that the data received over the connection is securely transmitted and protected from tampering or eavesdropping.
- **Buffer Size**: You should always check the return value of `SSL_read()`. The number of bytes read may be less than the buffer size (`num`), especially if the server sends fragmented or partial data.

### Common Errors and Troubleshooting:
- **SSL_ERROR_WANT_READ**: If `SSL_read()` returns `-1` with this error, it means the operation needs to wait for more data to arrive (in non-blocking I/O scenarios). You should try reading again when data is available.
- **SSL_ERROR_SSL**: This indicates an SSL-specific error (e.g., a handshake issue). You can call `SSL_get_error()` to get more details about the specific error.
- **SSL_ERROR_ZERO_RETURN**: This error indicates that the connection has been closed gracefully (i.e., the peer has closed the connection with SSL/TLS).

---

---

# What is SSL_shutdown()?

Good question — let's go through it carefully.

---

`SSL_shutdown()` is the OpenSSL function used to **properly close** an SSL/TLS connection **securely**.

It doesn't just close the socket abruptly; it **negotiates a clean shutdown** of the SSL/TLS session between the two sides (client and server). This way, both parties know the connection is ending and can securely finish the communication without any data loss or security risks.

---

### Why can't you just `close()` the socket?

- If you simply `close(fd)` on the socket without calling `SSL_shutdown()`, the other side might still think the connection is active, or worse — it might think the connection was **hijacked** or interrupted.
- **SSL/TLS is a stateful protocol**, meaning both parties maintain a session state. If you want to close it cleanly, you need to tell the other side "I am done sending and receiving" according to the SSL/TLS protocol rules.
- `SSL_shutdown()` sends a special "close_notify" alert message to the other party according to the SSL/TLS specification.

---

### Function Signature:
```c
int SSL_shutdown(SSL *ssl);
```

- **`ssl`**: the SSL object you have been using for your session.

### Return values:
- **1**: Successful shutdown completed (both sides agreed to close).
- **0**: Shutdown is **not finished** yet — you need to call `SSL_shutdown()` **again**.
- **Negative (-1)**: An error occurred; you should call `SSL_get_error()` to find out what went wrong.

### So sometimes you call `SSL_shutdown()` twice?

Yes — because:
- SSL/TLS shutdown is **a two-step process**:
  1. You send a "close_notify" alert.
  2. You receive a "close_notify" alert from the peer.
  
If only one side has sent their "close_notify," `SSL_shutdown()` returns `0`, and you need to call it again to complete the shutdown after receiving the peer's "close_notify."

---

### Simple Usage Example:
```c
// Proper shutdown
if (SSL_shutdown(ssl) == 0) {
    // Shutdown was not yet finished, call again
    SSL_shutdown(ssl);
}

// Now free the SSL object and close the socket
SSL_free(ssl);
close(fd);
```

---

### In short:
- `SSL_shutdown()` politely tells the peer, **"Hey, I'm done. You can shut down too."**
- It ensures no data is lost, no weird errors happen, and both sides securely agree that the session is closed.
- After `SSL_shutdown()`, you still need to call `SSL_free()` to free the SSL object, and `close()` the socket.

---

Would you like me to show you a full little "SSL client from connect to shutdown" mini example in C to put all this together? 🚀
(It's very clean once you see it.)# What is SSL_shutdown()?

Good question — let's go through it carefully.

---

`SSL_shutdown()` is the OpenSSL function used to **properly close** an SSL/TLS connection **securely**.

It doesn't just close the socket abruptly; it **negotiates a clean shutdown** of the SSL/TLS session between the two sides (client and server). This way, both parties know the connection is ending and can securely finish the communication without any data loss or security risks.

---

### Why can't you just `close()` the socket?

- If you simply `close(fd)` on the socket without calling `SSL_shutdown()`, the other side might still think the connection is active, or worse — it might think the connection was **hijacked** or interrupted.
- **SSL/TLS is a stateful protocol**, meaning both parties maintain a session state. If you want to close it cleanly, you need to tell the other side "I am done sending and receiving" according to the SSL/TLS protocol rules.
- `SSL_shutdown()` sends a special "close_notify" alert message to the other party according to the SSL/TLS specification.

---

### Function Signature:
```c
int SSL_shutdown(SSL *ssl);
```

- **`ssl`**: the SSL object you have been using for your session.

### Return values:
- **1**: Successful shutdown completed (both sides agreed to close).
- **0**: Shutdown is **not finished** yet — you need to call `SSL_shutdown()` **again**.
- **Negative (-1)**: An error occurred; you should call `SSL_get_error()` to find out what went wrong.

### So sometimes you call `SSL_shutdown()` twice?

Yes — because:
- SSL/TLS shutdown is **a two-step process**:
  1. You send a "close_notify" alert.
  2. You receive a "close_notify" alert from the peer.
  
If only one side has sent their "close_notify," `SSL_shutdown()` returns `0`, and you need to call it again to complete the shutdown after receiving the peer's "close_notify."

---

### Simple Usage Example:
```c
// Proper shutdown
if (SSL_shutdown(ssl) == 0) {
    // Shutdown was not yet finished, call again
    SSL_shutdown(ssl);
}

// Now free the SSL object and close the socket
SSL_free(ssl);
close(fd);
```

---

### In short:
- `SSL_shutdown()` politely tells the peer, **"Hey, I'm done. You can shut down too."**
- It ensures no data is lost, no weird errors happen, and both sides securely agree that the session is closed.
- After `SSL_shutdown()`, you still need to call `SSL_free()` to free the SSL object, and `close()` the socket.

---

Would you like me to show you a full little "SSL client from connect to shutdown" mini example in C to put all this together? 🚀
(It's very clean once you see it.)# What is SSL_shutdown()?

Good question — let's go through it carefully.

---

`SSL_shutdown()` is the OpenSSL function used to **properly close** an SSL/TLS connection **securely**.

It doesn't just close the socket abruptly; it **negotiates a clean shutdown** of the SSL/TLS session between the two sides (client and server). This way, both parties know the connection is ending and can securely finish the communication without any data loss or security risks.

---

### Why can't you just `close()` the socket?

- If you simply `close(fd)` on the socket without calling `SSL_shutdown()`, the other side might still think the connection is active, or worse — it might think the connection was **hijacked** or interrupted.
- **SSL/TLS is a stateful protocol**, meaning both parties maintain a session state. If you want to close it cleanly, you need to tell the other side "I am done sending and receiving" according to the SSL/TLS protocol rules.
- `SSL_shutdown()` sends a special "close_notify" alert message to the other party according to the SSL/TLS specification.

---

### Function Signature:
```c
int SSL_shutdown(SSL *ssl);
```

- **`ssl`**: the SSL object you have been using for your session.

### Return values:
- **1**: Successful shutdown completed (both sides agreed to close).
- **0**: Shutdown is **not finished** yet — you need to call `SSL_shutdown()` **again**.
- **Negative (-1)**: An error occurred; you should call `SSL_get_error()` to find out what went wrong.

### So sometimes you call `SSL_shutdown()` twice?

Yes — because:
- SSL/TLS shutdown is **a two-step process**:
  1. You send a "close_notify" alert.
  2. You receive a "close_notify" alert from the peer.
  
If only one side has sent their "close_notify," `SSL_shutdown()` returns `0`, and you need to call it again to complete the shutdown after receiving the peer's "close_notify."

---

### Simple Usage Example:
```c
// Proper shutdown
if (SSL_shutdown(ssl) == 0) {
    // Shutdown was not yet finished, call again
    SSL_shutdown(ssl);
}

// Now free the SSL object and close the socket
SSL_free(ssl);
close(fd);
```

---

---

# Clean up

Alright — let’s break it down clearly:

---

### `SSL_free(SSL *ssl)`
This **frees** (cleans up) an individual SSL connection **object**.

- When you call `SSL_new(ctx)`, OpenSSL **allocates memory** for a specific connection (`SSL *ssl`).
- After you are done using that connection (you've finished talking and done `SSL_shutdown()`), you **must** call `SSL_free(ssl)` to **free** all the memory, buffers, and internal structures associated with that single connection.
- If you don’t `SSL_free()`, you **leak memory**.

🔹 **In simple words**:  
→ "`SSL_free()` destroys the SSL connection object you used for one session."

---

### `SSL_CTX_free(SSL_CTX *ctx)`
This **frees** (cleans up) the **context** object, which holds **global settings** for SSL connections.

- When you initialize SSL (for example, with `SSL_CTX_new(method)`), you create a **context**.
- This `SSL_CTX` holds **configuration** used for **many** SSL connections: like certificates, private keys, ciphers to use, verification settings, etc.
- After **all** your SSL connections using this context are finished and you don't need it anymore, you **must** call `SSL_CTX_free(ctx)` to **free** that context's memory and clean up.

🔹 **In simple words**:  
→ "`SSL_CTX_free()` destroys the SSL context object that could have been shared by many connections."

---

### Very important rule:  
- **Every `SSL_new()` must be matched with an `SSL_free()`.**
- **Every `SSL_CTX_new()` must be matched with an `SSL_CTX_free()`.**

(Otherwise your program leaks memory.)

---

---

# Close the socket file descriptor

Once you have:
- **`SSL_shutdown(ssl)`** → cleanly closed the SSL/TLS session,
- **`SSL_free(ssl)`** → freed the SSL connection object,
- **(and optionally, later) `SSL_CTX_free(ctx)`** → freed the SSL context if you're done with all SSL connections,

**then you can safely do:**

```c
close(sockFd);
```

to **close the raw TCP socket** underneath.

---

### Quick mental picture of the flow:
1. `socket()` → create the TCP socket (`sockFd`)
2. `connect(sockFd, ...)` → connect TCP to server
3. `SSL_set_fd(ssl, sockFd)` → tell OpenSSL to *use* that socket
4. `SSL_connect(ssl)` → start SSL/TLS handshake over that socket
5. `SSL_write(ssl)` / `SSL_read(ssl)` → securely send/receive encrypted data
6. `SSL_shutdown(ssl)` → **properly close SSL/TLS** session
7. `SSL_free(ssl)` → **free** SSL connection memory
8. `close(sockFd)` → **close** the actual raw TCP socket

(And at the very end of your program, you also call `SSL_CTX_free(ctx)` if you're completely done with SSL.)

---

---
