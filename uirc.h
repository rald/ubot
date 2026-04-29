#ifndef MINI_IRC_H
#define MINI_IRC_H

/* Define Windows Version (0x0600 = Vista+) to enable getaddrinfo */
#if defined(_WIN32) || defined(_WIN64)
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600
    #endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* --- OS-Specific Headers and Typedefs --- */
#if defined(_WIN32) || defined(_WIN64)
    #define IRC_OS_WINDOWS
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET irc_socket_t;
#else
    #define IRC_OS_UNIX
    #include <sys/socket.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    #include <arpa/inet.h>
    typedef int irc_socket_t;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

typedef struct {
    irc_socket_t fd;
    char *buf;    
    size_t len;   
    size_t cap;   
} irc_t;

int  irc_connect(irc_t *irc, const char *host, const char *port);
void irc_vsend(irc_t *irc, const char *fmt, va_list ap);
void irc_send(irc_t *irc, const char *fmt, ...);
int  irc_recv_line(irc_t *irc, char *out_buf, size_t out_len);
void irc_close(irc_t *irc);

#endif 

#ifdef IRC_IMPLEMENTATION

int irc_connect(irc_t *irc, const char *host, const char *port) {
#ifdef IRC_OS_WINDOWS
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return -1;
#endif

    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    irc->len = 0;
    irc->cap = 1024;
    irc->buf = (char*)malloc(irc->cap);
    if (!irc->buf) return -1;

    if (getaddrinfo(host, port, &hints, &res) != 0) {
        free(irc->buf);
        return -1;
    }

    irc->fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (irc->fd == INVALID_SOCKET) {
        freeaddrinfo(res);
        free(irc->buf);
        return -1;
    }

    if (connect(irc->fd, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        irc_close(irc);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return 0;
}

void irc_vsend(irc_t *irc, const char *fmt, va_list ap) {
    va_list aq;
    va_copy(aq, ap);
    int n = vsnprintf(NULL, 0, fmt, aq);
    va_end(aq);
    if (n < 0) return;

    char *msg = (char*)malloc(n + 3);
    if (!msg) return;

    vsnprintf(msg, n + 1, fmt, ap);
    memcpy(msg + n, "\r\n", 3);
    n += 2;

    int total = 0;
    while (total < n) {
        int sent = send(irc->fd, msg + total, n - total, 0);
        if (sent <= 0) break;
        total += sent;
    }
    free(msg);
}

void irc_send(irc_t *irc, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    irc_vsend(irc, fmt, ap);
    va_end(ap);
}

int irc_recv_line(irc_t *irc, char *out_buf, size_t out_len) {
    while (1) {
        char *nl = (char*)memchr(irc->buf, '\n', irc->len);
        if (nl) {
            size_t line_len = nl - irc->buf + 1;
            size_t copy_len = (line_len < out_len - 1) ? line_len : out_len - 1;
            memcpy(out_buf, irc->buf, copy_len);
            out_buf[copy_len] = '\0';
            irc->len -= line_len;
            memmove(irc->buf, nl + 1, irc->len);
            return (int)copy_len;
        }
        if (irc->len >= irc->cap) {
            size_t new_cap = irc->cap * 2;
            char *new_buf = (char*)realloc(irc->buf, new_cap);
            if (!new_buf) return -1;
            irc->buf = new_buf;
            irc->cap = new_cap;
        }
        int n = recv(irc->fd, irc->buf + irc->len, (int)(irc->cap - irc->len), 0);
        if (n <= 0) return n;
        irc->len += n;
    }
}

void irc_close(irc_t *irc) {
    if (irc->buf) { free(irc->buf); irc->buf = NULL; }
    if (irc->fd != INVALID_SOCKET) {
#ifdef IRC_OS_WINDOWS
        closesocket(irc->fd);
        WSACleanup();
#else
        close(irc->fd);
#endif
        irc->fd = INVALID_SOCKET;
    }
}
#endif