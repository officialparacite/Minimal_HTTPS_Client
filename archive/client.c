#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <ctype.h>

#define BUF_SIZE 4096
#define MAX_REDIRECTS 5

void urlParser(const char* url, char* hostname, char* path);
int connectToHost(const char* hostname, const char* port);
void sendRequest(int fd, const void *buf, size_t len);
int parseRedirectLocation(const char* response, char* location, size_t max_len);

int main(int argc, char* argv[]) {
    int follow_redirects = 0;
    char *port = "80";

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
    char response_buffer[BUF_SIZE * 4]; // Larger buffer for accumulated response
    
    do {
        char hostname[BUF_SIZE];
        char path[BUF_SIZE];
        urlParser(currentUrl, hostname, path);
        
        printf("Connecting to %s, path: %s, port: %s\n", hostname, path, port);

        int sockFd = connectToHost(hostname, port);

        char request[BUF_SIZE];
        snprintf(request, sizeof(request),
                 "GET /%s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "Connection: close\r\n"
                 "\r\n", path, hostname);

        sendRequest(sockFd, request, strlen(request));

        ssize_t bytesRead;
        size_t total_bytes = 0;
        char buffer[BUF_SIZE];
        
        // Clear response buffer for new request
        memset(response_buffer, 0, sizeof(response_buffer));

        // Read the entire response
        while ((bytesRead = read(sockFd, buffer, BUF_SIZE - 1)) > 0) {
            buffer[bytesRead] = '\0';
            
            // Check if we have room in the response buffer
            if (total_bytes + bytesRead < sizeof(response_buffer)) {
                memcpy(response_buffer + total_bytes, buffer, bytesRead);
                total_bytes += bytesRead;
            } else {
                fprintf(stderr, "Response too large for buffer\n");
                break;
            }
        }

        if (bytesRead == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        close(sockFd);

        int redirect = 0;
        char new_location[BUF_SIZE] = {0};
        
        // Check for redirects if flag is set
        if (follow_redirects) {
            if (parseRedirectLocation(response_buffer, new_location, BUF_SIZE)) {
                printf("Redirecting to: %s\n", new_location);
                strncpy(currentUrl, new_location, BUF_SIZE - 1);
                redirect = 1;
                redirect_count++;
                
                if (redirect_count > MAX_REDIRECTS) {
                    fprintf(stderr, "Too many redirects, giving up\n");
                    exit(EXIT_FAILURE);
                }
            }
        }

        // If not redirecting or we've reached final destination, display the response
        if (!redirect || !follow_redirects) {
            if (write(STDOUT_FILENO, response_buffer, total_bytes) == -1) {
                perror("write");
                exit(EXIT_FAILURE);
            }
            break;
        }

    } while (follow_redirects);

    exit(EXIT_SUCCESS);
}

void urlParser(const char* url, char* hostname, char* path) {
    const char* start = url;
    
    // Skip protocol part if present
    if (strncmp(url, "http://", 7) == 0) {
        start = url + 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        start = url + 8;
    }

    // Copy hostname and path
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

int connectToHost(const char* hostname, const char* port) {
    struct addrinfo hints, *res, *p;
    int sockFd;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(res);

    return sockFd;
}

void sendRequest(int fd, const void *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t bytesWrote = write(fd, (char*)buf + sent, len - sent);
        if (bytesWrote == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
        sent += bytesWrote;
    }
}

int parseRedirectLocation(const char* response, char* location, size_t max_len) {
    // First check if we have a redirect status code (301 or 302)
    if (strstr(response, "HTTP/1.1 301") || 
        strstr(response, "HTTP/1.1 302") || 
        strstr(response, "HTTP/1.0 301") || 
        strstr(response, "HTTP/1.0 302")) {
        
        const char* loc_header = strcasestr(response, "Location:");
        if (loc_header) {
            loc_header += 9; // Skip "Location:"
            
            // Skip whitespace
            while (*loc_header && isspace(*loc_header)) loc_header++;
            
            // Copy until end of line or max length
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
