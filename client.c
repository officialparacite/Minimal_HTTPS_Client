#include <openssl/crypto.h>
#include <openssl/prov_ssl.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <strings.h>

#define BUF_SIZE 100000
#define MAX_REDIRECTS 5

static void errMsg(const char* msg);
static void urlParser(const char* url, char* hostname, char* path);
static int lookupDNS(const char* hostname, const char* port);
static int parseRedirectLocation(const char* response, char* location, size_t max_len);

int main(int argc, char* argv[]) {
    int follow_redirects = 0;
    char* port = "443";

    int opt;
    while ((opt = getopt(argc, argv, "rp:")) != -1) {
        switch (opt) {
            case 'r':
                follow_redirects = 1;
                break;
            case 'p':
                port = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [-r] [-p <port>] URL\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected URL after options\n");
        exit(EXIT_FAILURE);
    }

    char currentUrl[BUF_SIZE];
    snprintf(currentUrl, BUF_SIZE, "%s", argv[optind]);

    int redirect_count = 0;
    char responseBuffer[BUF_SIZE * 4];

    do {
        char hostname[BUF_SIZE];
        char path[BUF_SIZE];
        urlParser(currentUrl, hostname, path);

        printf("Connecting to %s, path: %s, port: %s\n", hostname, path, port);

        int sockFd = lookupDNS(hostname, port);
        if (sockFd < 0) {
            fprintf(stderr, "Failed to establish connection\n");
            exit(EXIT_FAILURE);
        }

        const SSL_METHOD* method = TLS_client_method();
        SSL_CTX* ctx = SSL_CTX_new(method);
        if (!ctx) {
            errMsg("SSL_CTX_new() failed");
            close(sockFd);
            exit(EXIT_FAILURE);
        }

        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
        SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);

        if (!SSL_CTX_set_default_verify_paths(ctx)) {
            errMsg("Failed to set default verify paths\n");
            SSL_CTX_free(ctx);
            close(sockFd);
            exit(EXIT_FAILURE);
        }

        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

        SSL *ssl = SSL_new(ctx);
        if (!ssl) {
            errMsg("SSL_new() failed");
            SSL_CTX_free(ctx);
            close(sockFd);
            exit(EXIT_FAILURE);
        }

        if (!SSL_set_tlsext_host_name(ssl, hostname)) {
            errMsg("SSL_set_tlsext_host_name() failed");
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sockFd);
            exit(EXIT_FAILURE);
        }

        if (!SSL_set1_host(ssl, hostname)) {
            errMsg("SSL_set1_host() failed");
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sockFd);
            exit(EXIT_FAILURE);
        }

        SSL_set_fd(ssl, sockFd);

        if (SSL_connect(ssl) != 1) {
            errMsg("SSL_connect() failed");
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sockFd);
            exit(EXIT_FAILURE);
        }

        char request[BUF_SIZE];
        snprintf(request, sizeof(request),
                 "GET /%s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "Connection: close\r\n"
                 "\r\n", path, hostname);

        if (SSL_write(ssl, request, strlen(request)) <= 0) {
            errMsg("SSL_write() failed");
            SSL_shutdown(ssl);
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sockFd);
            exit(EXIT_FAILURE);
        }

        ssize_t bytesRead;
        size_t total_bytes = 0;
        char buffer[BUF_SIZE];

        memset(responseBuffer, 0, sizeof(responseBuffer));

        while ((bytesRead = SSL_read(ssl, buffer, BUF_SIZE)) > 0) {
            if (total_bytes + bytesRead < sizeof(responseBuffer)) {
                memcpy(responseBuffer + total_bytes, buffer, bytesRead);
                total_bytes += bytesRead;
            } else {
                fprintf(stderr, "Response too large for buffer\n");
                break;
            }
        }

        if (bytesRead < 0) {
            errMsg("SSL_read() failed");
            SSL_shutdown(ssl);
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            close(sockFd);
            exit(EXIT_FAILURE);
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sockFd);

        int redirect = 0;
        char new_location[BUF_SIZE] = {0};

        if (follow_redirects) {
            if (parseRedirectLocation(responseBuffer, new_location, BUF_SIZE)) {
                printf("Redirecting to %s\n", new_location);
                strncpy(currentUrl, new_location, BUF_SIZE - 1);
                currentUrl[BUF_SIZE - 1] = '\0';
                redirect = 1;
                redirect_count++;

                if (redirect_count > MAX_REDIRECTS) {
                    fprintf(stderr, "Too many redirects, giving up\n");
                    exit(EXIT_FAILURE);
                }
            }
        }

        if (!redirect || !follow_redirects) {
            if (write(STDOUT_FILENO, responseBuffer, total_bytes) == -1) {
                perror("write");
                exit(EXIT_FAILURE);
            }
            break;
        }

    } while (follow_redirects);

    return EXIT_SUCCESS;
}

static void errMsg(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    ERR_print_errors_fp(stderr);
}

static void urlParser(const char* url, char* hostname, char* path) {
    const char* start = url;

    if (strncmp(url, "http://", 7) == 0) {
        start = url + 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        start = url + 8;
    }

    const char* slash = strchr(start, '/');
    if (slash != NULL) {
        size_t hostname_len = slash - start;
        strncpy(hostname, start, hostname_len);
        hostname[hostname_len] = '\0';
        strcpy(path, slash + 1);
    } else {
        strcpy(hostname, start);
        path[0] = '\0';
    }
}

static int lookupDNS(const char* hostname, const char* port) {
    struct addrinfo hints, *res, *p;
    int sockFd;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sockFd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockFd == -1) continue;

        if (connect(sockFd, p->ai_addr, p->ai_addrlen) == 0) break;

        close(sockFd);
    }

    if (p == NULL) {
        fprintf(stderr, "Failed to connect to %s:%s\n", hostname, port);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return sockFd;
}

static int parseRedirectLocation(const char* response, char* location, size_t max_len) {
    if (strstr(response, "HTTP/1.1 301") || 
        strstr(response, "HTTP/1.1 302") || 
        strstr(response, "HTTP/1.0 301") || 
        strstr(response, "HTTP/1.0 302")) {

        const char* loc_header = strcasestr(response, "Location:");
        if (loc_header) {
            loc_header += 9;

            while (*loc_header && isspace(*loc_header)) loc_header++;

            size_t i = 0;
            while (i < max_len - 1 && *loc_header && *loc_header != '\r' && *loc_header != '\n') {
                location[i++] = *loc_header++;
            }
            location[i] = '\0';
            return 1;
        }
    }
    return 0;
}
