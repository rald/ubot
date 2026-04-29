#ifndef UIRC_H
#define UIRC_H

#ifdef _WIN32
    #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
    #define POLL_ERR_SET (POLLHUP | POLLERR | POLLNVAL)
#else
    #define _POSIX_C_SOURCE 200112L
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <signal.h> 
    #include <errno.h>
    typedef int SOCKET;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    SOCKET fd;
} uirc_t;

int  uirc_connect(uirc_t *uirc, const char *host, const char *port);
void uirc_send(uirc_t *uirc, const char *fmt, ...);
void uirc_vsend(uirc_t *uirc, const char *fmt, va_list ap);
int  uirc_recv_line(uirc_t *uirc, char *out_buf, size_t out_len);
void uirc_close(uirc_t *uirc);

#endif

#ifdef UIRC_IMPLEMENTATION

int uirc_connect(uirc_t *uirc, const char *host, const char *port) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
#endif

    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    uirc->len = 0;
    uirc->cap = 8192;
    uirc->buf = (char*)malloc(uirc->cap);
    if (!uirc->buf) return -1;

    if (getaddrinfo(host, port, &hints, &res) != 0) {
        free(uirc->buf);
        return -1;
    }

    uirc->fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (uirc->fd == INVALID_SOCKET) {
        freeaddrinfo(res);
        free(uirc->buf);
        return -1;
    }

#ifdef SO_NOSIGPIPE
    int set = 1;
    setsockopt(uirc->fd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&set, sizeof(int));
#endif

    if (connect(uirc->fd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(res);
        uirc_close(uirc);
        return -1;
    }

    freeaddrinfo(res);
    return 0;
}

void uirc_vsend(uirc_t *uirc, const char *fmt, va_list ap) {
    char buf[514];
    va_list aq;
    va_copy(aq, ap);
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, aq);
    va_end(aq);

    if (n < 0) return;
    if (n > 512) n = 512;

    memcpy(buf + n, "\r\n", 2);
    n += 2;

    int sent = 0;
    while (sent < n) {
        int flags = 0;
#ifndef _WIN32
    #ifdef MSG_NOSIGNAL
        flags = MSG_NOSIGNAL;
    #endif
#endif
        int res = send(uirc->fd, buf + sent, n - sent, flags);
        
        if (res == SOCKET_ERROR) {
#ifndef _WIN32
            if (errno == EINTR) continue;
#endif
            return; // Error or connection lost
        }
        sent += res;
    }
}

void uirc_send(uirc_t *uirc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    uirc_vsend(uirc, fmt, ap);
    va_end(ap);
}

int uirc_recv_line(uirc_t *uirc, char *out_buf, size_t out_len) {
    while (1) {
        // Check if we already have a full line in our internal buffer
        char *nl = (char*)memchr(uirc->buf, '\n', uirc->len);
        if (nl) {
            size_t line_len = (size_t)(nl - uirc->buf + 1);
            size_t copy_len = (line_len >= out_len) ? out_len - 1 : line_len;

            memcpy(out_buf, uirc->buf, copy_len);
            out_buf[copy_len] = '\0';

            uirc->len -= line_len;
            if (uirc->len > 0) {
                memmove(uirc->buf, nl + 1, uirc->len);
            }
            return (int)copy_len;
        }

        // If buffer is full and no newline found, we must grow it
        if (uirc->len >= uirc->cap) {
            size_t new_cap = uirc->cap * 2;
            char *tmp = (char*)realloc(uirc->buf, new_cap);
            if (!tmp) return -1;
            uirc->buf = tmp;
            uirc->cap = new_cap;
        }

        // Receive more data into the end of our buffer
        int n = (int)recv(uirc->fd, uirc->buf + uirc->len, (int)(uirc->cap - uirc->len), 0);
        
        if (n < 0) {
#ifndef _WIN32
            if (errno == EINTR) continue; // Interrupted by signal, try again
#endif
            return -1; // Actual error
        }
        if (n == 0) return 0; // Remote closed connection
        
        uirc->len += n;
    }
}

void uirc_close(uirc_t *uirc) {
    if (uirc->buf) { 
        free(uirc->buf); 
        uirc->buf = NULL; 
    }
    if (uirc->fd != INVALID_SOCKET) {
        closesocket(uirc->fd);
#ifdef _WIN32
        WSACleanup();
#endif
        uirc->fd = INVALID_SOCKET;
    }
}
#endif