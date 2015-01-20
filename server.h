/**
 * Dummy HTTP server. Use optional first argument to set port number
 * (defaults to 8080).
 */

#ifndef SERVER
#define SERVER

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <ev.h>


// configuration constants

// the default ulimit -Sn is usually 1024, but
// take the default file descriptors into account
#define BACKLOG_SIZE    1000
#define BUFFER_SIZE     4096
#define POOL_SIZE       1000
#define DEFAULT_SERVICE "8080"


// data types

struct request {
    char version;
    long content_length;
};

struct handler {
    int next;
    int state;
    struct request request;

    char data[BUFFER_SIZE];
    int size;
    int mark;
    int fd;

    struct ev_io watcher;
};


// macros

#define TRUE  1
#define FALSE 0

#ifdef DEBUG
#define debug(...) do { \
    printf(__VA_ARGS__); \
    putchar('\n'); \
} while (0)

#else
#define debug(...) do {} while (0)

#endif


// functions

/**
 * Reads data from the socket to the request buffer. The marker is reset to
 * 0, since old data is discarded. Returns zero on error or EOF.
 */
int read_socket(struct handler*);

#endif

