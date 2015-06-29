/**
 * Dummy HTTP server. Use optional first argument to set port number
 * (defaults to 8080).
 */

#ifndef SERVER
#define SERVER

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <netdb.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <ev.h>

#include "errors.h"
#include "parser.h"

// data types

/**
 * Request handler state.
 */
struct handler {
    int state;
    int error;
    struct server* pool;
    struct handler* next;
    struct ev_io watcher;
    struct parser parser;
    int fd;
};

/**
 * Server state.
 */
struct server {
    int socket;
    int handler_count;
    struct handler* handler_pool;
};


// macros

#define ST_ERROR    0
#define ST_DONE     1
#define ST_WAITING  2
#define ST_READING  3
#define ST_WRITING  4

#endif

