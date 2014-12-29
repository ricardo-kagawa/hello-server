/**
 * Dummy HTTP server. Use optional first argument to set port number
 * (defaults to 8080).
 */

#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

// the default ulimit -Sn is usually 1024, but
// take the default file descriptors into account
#define BACKLOG_SIZE    1000
#define DEFAULT_SERVICE "8080"

#define E_NONE      0
#define E_ADDRINFO  1
#define E_SSOCKET   2
#define E_SS_BIND   3
#define E_SS_LISTEN 4

/**
 * Outputs a corresponding error message to stderr.
 */
void error(int cat, int code) {
    switch(cat) {
        case E_ADDRINFO:
            fputs(gai_strerror(code), stderr);
            break;

        case E_SSOCKET:
            fputs("Failed to open server socket", stderr);
            switch(code) {
                case EACCES:
                    fputs(": access denied", stderr);
                    break;
                case ENOBUFS:
                case ENOMEM:
                    fputs(": out of memory", stderr);
                    break;
            }
            break;

        case E_SS_BIND:
            fputs("Failed to bind server socket", stderr);
            switch(code) {
                case EACCES:
                    fputs(": access denied", stderr);
                    break;
                case EADDRINUSE:
                    fputs(": address already in use", stderr);
                    break;
            }
            break;

        case E_SS_LISTEN:
            fputs("Failed to listen on server socket", stderr);
            if (code == EADDRINUSE)
                fputs(": address already in use", stderr);
            break;
    }
    fputc('\n', stderr);
}

/**
 * Calls getaddrinfo(3) with hints suitable for an HTTP server. _service_
 * can be a service name (see services(5)) or a decimal port number. The
 * returned address info must be released with freeaddrinfo(3). On error,
 * NULL is returned and some message is output to _stderr_.
 */
struct addrinfo* get_server_addrinfo(char service[]) {
    struct addrinfo h, *r;
    int rv;

    h.ai_flags = AI_PASSIVE;
    h.ai_family = AF_UNSPEC;
    h.ai_socktype = SOCK_STREAM;
    h.ai_protocol = 0;
    h.ai_addrlen = 0;
    h.ai_addr = NULL;
    h.ai_canonname = NULL;
    h.ai_next = NULL;

    rv = getaddrinfo(NULL, service, &h, &r);
    if (rv != 0) {
        error(E_ADDRINFO, rv);
        return NULL;
    } else
        return r;
}

/**
 * Opens a listening server socket to accept TCP connections. The socket's
 * file descriptor is returned on success. _service_ can be a service name
 * (see services(5)) or a decimal port number.
 */
int open_server_socket(char service[]) {
    struct addrinfo *ai, *aip;
    int fd;

    ai = get_server_addrinfo(service);

    aip = ai;
    while (aip != NULL) {
        fd = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);

        if (fd < 0) {
            error(E_SSOCKET, errno);
            aip = aip->ai_next;
            continue;
        }

        if (bind(fd, aip->ai_addr, aip->ai_addrlen) != 0) {
            error(E_SS_BIND, errno);
            aip = aip->ai_next;
            continue;
        }

        if (listen(fd, BACKLOG_SIZE) != 0) {
            error(E_SS_LISTEN, errno);
            aip = aip->ai_next;
            continue;
        }

        if (ai != NULL)
            freeaddrinfo(ai);
        return fd;
    }

    if (ai != NULL)
        freeaddrinfo(ai);
    return -1;
}

int main(int argc, char** argv) {
    int ssocket, socket;

    ssocket = open_server_socket((argc > 1) ? argv[1] : DEFAULT_SERVICE);
    puts("server is ready");

    socket = accept(ssocket, NULL, NULL);
    printf("%d\n", socket);

    close(socket);
    close(ssocket);
    return 0;
}

