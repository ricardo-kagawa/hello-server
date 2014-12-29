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

#define DEFAULT_SERVICE "8080"

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
        fputs(gai_strerror(EAI_AGAIN), stderr);
        fputc('\n', stderr);
        return NULL;
    } else
        return r;
}

/**
 * Creates an unbound server socket using the given address info. If the
 * socket could not be created, an error message is printed to stderr and
 * -1 is returned.
 */
int create_server_socket(struct addrinfo *ai) {
    int fd, ecode;

    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0) {
        ecode = errno;
        fputs("Could not open server socket", stderr);
        switch(ecode) {
            case EACCES:
                fputs(": access denied\n", stderr);
                break;
            case ENOBUFS:
            case ENOMEM:
                fputs(": out of memory\n", stderr);
                break;
            default:
                fputc('\n', stderr);
        }
        return -1;
    } else
        return fd;
}

int open_server_socket(char service[]) {
    struct addrinfo *ai, *aip;
    int fd;

    ai = get_server_addrinfo(service);

    aip = ai;
    while (aip != NULL) {
        fd = create_server_socket(aip);
        aip = aip->ai_next;

        if (fd < 0)
            continue;

        if (ai != NULL)
            freeaddrinfo(ai);
        return fd;
    }
    return -1;
}

int main(int argc, char** argv) {
    int ssocket;

    ssocket = open_server_socket((argc > 1) ? argv[1] : DEFAULT_SERVICE);
    printf("ssocket=%d\n", ssocket);

    close(ssocket);
    return 0;
}

