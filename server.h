/**
 * Dummy HTTP server. Use optional first argument to set port number
 * (defaults to 8080).
 */

#ifndef SERVER
#define SERVER


// configuration constants

// the default ulimit -Sn is usually 1024, but
// take the default file descriptors into account
#define BACKLOG_SIZE    1000
#define BUFFER_SIZE     4096
#define DEFAULT_SERVICE "8080"


// data types

struct request {
    char data[BUFFER_SIZE];
    int size;
    int mark;
    int fd;
};


// macros

#define TRUE  1
#define FALSE 0

#define min(a, b) ((a) < (b) ? (a) : (b))


// functions

/**
 * Reads data from the socket to the request buffer. The marker is reset to
 * 0, since old data is discarded. Returns zero on error or EOF.
 */
int read_socket(struct request*);

#endif
