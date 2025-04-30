#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

void print_error(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    ERR_print_errors_fp(stderr);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <hostname>\n", argv[0]);
        return 1;
    }
    
    const char *hostname = argv[1];
    const char *portnum = "443";
    int ret = 1;
    
    // In OpenSSL 1.1.0+ these initialization calls are not needed
    // as initialization is done automatically
    
    // --- Resolve hostname ---
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; // IPv4 only
    hints.ai_socktype = SOCK_STREAM;
    
    if (getaddrinfo(hostname, portnum, &hints, &res) != 0) {
        perror("getaddrinfo");
        return 1;
    }
    
    // --- Create socket and connect ---
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }
    
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
        perror("connect");
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }
    
    // --- Set up SSL context ---
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        print_error("SSL_CTX_new() failed");
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }
    
    // Modern security options
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION); // Minimum TLS 1.2
    SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);    // Disable compression
    
    // Use system's default CA store
    if (!SSL_CTX_set_default_verify_paths(ctx)) {
        print_error("Failed to set default verify paths");
        SSL_CTX_free(ctx);
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }
    
    // Enable certificate verification
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    
    // --- Create SSL object and attach to socket ---
    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        print_error("SSL_new() failed");
        SSL_CTX_free(ctx);
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }
    
    // Set hostname for SNI (Server Name Indication)
    if (!SSL_set_tlsext_host_name(ssl, hostname)) {
        print_error("SSL_set_tlsext_host_name() failed");
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }
    
    // Set hostname for certificate verification
    if (!SSL_set1_host(ssl, hostname)) {
        print_error("SSL_set1_host() failed");
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }
    
    SSL_set_fd(ssl, sockfd);
    
    // --- Perform TLS handshake ---
    if (SSL_connect(ssl) != 1) {
        print_error("SSL_connect() failed");
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sockfd);
        freeaddrinfo(res);
        return 1;
    }
    
    // Print connection info
    printf("Connected with %s encryption\n", SSL_get_cipher(ssl));
    printf("Protocol: %s\n", SSL_get_version(ssl));
    
    // Verify the certificate
    long verify_result = SSL_get_verify_result(ssl);
    if (verify_result == X509_V_OK) {
        printf("Certificate verification: SUCCESS\n");
        
        // Get certificate information
        X509 *cert = SSL_get_peer_certificate(ssl);
        if (cert) {
            char subject_name[256];
            X509_NAME_oneline(X509_get_subject_name(cert), subject_name, sizeof(subject_name));
            printf("Subject: %s\n", subject_name);
            
            char issuer_name[256];
            X509_NAME_oneline(X509_get_issuer_name(cert), issuer_name, sizeof(issuer_name));
            printf("Issuer: %s\n", issuer_name);
            
            X509_free(cert);
        } else {
            fprintf(stderr, "No certificate presented by peer\n");
        }
    } else {
        fprintf(stderr, "Certificate verification failed: %s\n", 
                X509_verify_cert_error_string(verify_result));
    }
    
    // --- Send HTTP GET request ---
    char request[1024];
    snprintf(request, sizeof(request),
             "GET / HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: OpenSSL-Client/1.0\r\n"
             "Accept: */*\r\n"
             "Connection: close\r\n"
             "\r\n", hostname);
    
    if (SSL_write(ssl, request, strlen(request)) <= 0) {
        print_error("SSL_write() failed");
        goto cleanup;
    }
    
    // --- Read and print HTTP response ---
    char buf[4096];
    int bytes;
    printf("\n--- HTTP Response ---\n");
    while ((bytes = SSL_read(ssl, buf, sizeof(buf) - 1)) > 0) {
        buf[bytes] = '\0';  // Null-terminate for printing as string
        printf("%s", buf);
    }
    
    if (bytes < 0) {
        print_error("SSL_read() failed");
    }
    
    ret = 0;  // Success

cleanup:
    // --- Clean up ---
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sockfd);
    freeaddrinfo(res);
    
    // In OpenSSL 1.1.0+, these cleanup calls are not needed
    // as cleanup is done automatically when the program exits
    
    return ret;
}
