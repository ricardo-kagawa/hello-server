#include "server.h"
#include "parser.c"


#define E_NONE     0
#define E_ADDRINFO 1
#define E_SOCKET   2
#define E_BIND     3
#define E_LISTEN   4
#define E_ACCEPT   5
#define E_HANDLER  6

#define HTTP_VERSION "HTTP/1."
#define RESP_200 " 200 OK\r\nContent-Length: 11\r\n\r\nhello world"
#define RESP_200H " 200 OK\r\nContent-Length: 11\r\n\r\n"
#define RESP_400 " 400 Bad Request\r\nContent-Length: 0\r\n\r\n"
#define RESP_501 " 501 Not Implemented\r\nContent-Length: 0\r\n\r\n"


// data types

/**
 * Pool of request handler state.
 */
struct handler_pool {
    struct handler handlers[POOL_SIZE];
    int next;
};

/**
 * Server state.
 */
struct server {
    int socket;
    struct handler_pool handler_pool;
};



/**
 * Outputs a corresponding error message to stderr.
 */
void error(int cat, int code) {
    switch (cat) {
    case E_ADDRINFO:
        fputs(gai_strerror(code), stderr);
        break;

    case E_SOCKET:
        fputs("Failed to open server socket", stderr);
        switch (code) {
        case EACCES:
            fputs(": access denied", stderr);
            break;
        case ENOBUFS:
        case ENOMEM:
            fputs(": out of memory", stderr);
            break;
        }
        break;

    case E_BIND:
        fputs("Failed to bind server socket", stderr);
        switch (code) {
        case EACCES:
            fputs(": access denied", stderr);
            break;
        case EADDRINUSE:
            fputs(": address already in use", stderr);
            break;
        }
        break;

    case E_LISTEN:
        fputs("Failed to listen on server socket", stderr);
        if (code == EADDRINUSE)
            fputs(": address already in use", stderr);
        break;

    case E_HANDLER:
        fputs("Failed to set custom signal handlers", stderr);
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
    int fd, type, val;

    debug("service: %s", service);
    ai = get_server_addrinfo(service);

    aip = ai;
    while (aip != NULL) {
        type = aip->ai_socktype | SOCK_NONBLOCK;
        fd = socket(aip->ai_family, type, aip->ai_protocol);

        if (fd < 0) {
            error(E_SOCKET, errno);
            aip = aip->ai_next;
            continue;
        }

        val = TRUE;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

        if (bind(fd, aip->ai_addr, aip->ai_addrlen) != 0) {
            error(E_BIND, errno);
            aip = aip->ai_next;
            continue;
        }

        if (listen(fd, BACKLOG_SIZE) != 0) {
            error(E_LISTEN, errno);
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

/**
 * Initialized the handler pool. The pool is a linked list of available
 * handlers.
 */
void init_handler_pool(struct handler_pool *pool) {
    int i;
    pool->next = 0;
    for (i = 0; i < POOL_SIZE - 1; i++)
        pool->handlers[i].next = i + 1;
    pool->handlers[POOL_SIZE - 1].next = -1;
}

/**
 * Retrieves the next free handler. If there is none, NULL is returned.
 */
struct handler* get_handler(struct handler_pool *pool) {
    struct handler *r;

    if (pool->next < 0)
        return NULL;

    r = pool->handlers + pool->next;

    if (r->next >= 0) {
        pool->next = r->next;
        r->next = -1;
    } else
        pool->next = -1;

    r->request.version = '0';
    r->request.content_length = 0;

    r->size = 0;
    r->mark = 0;
    r->fd = -1;

    return r;
}

// see header file
int read_socket(struct handler *h) {
    debug("reading socket");
    h->size = read(h->fd, h->data, sizeof(h->data));
    h->mark = 0;

    debug("%d bytes read", h->size);
    return (h->size > 0);
}

/**
 * Writes the given response with size bytes to the socket pointed to by req.
 * The corresponding HTTP minor version in req is written to the response.
 */
void write_response(struct handler *h, char resp[], int size) {
    write(h->fd, HTTP_VERSION, sizeof(HTTP_VERSION) - 1);
    write(h->fd, &(h->request.version), sizeof(char));
    write(h->fd, resp, size - 1);
}

/**
 * Handles a new client connection.
 */
void handle_connection(struct ev_loop *loop, struct server *server, 
        int socket) {
    struct handler *h;

    parse_request(h);
    switch (0) {
    case REQ_GET:
        write_response(h, RESP_200, sizeof(RESP_200));
        return;

    case REQ_HEAD:
        write_response(h, RESP_200H, sizeof(RESP_200H));
        return;

    case REQ_BAD_HTTP:
        write_response(h, RESP_400, sizeof(RESP_400));
        return;

    case REQ_BAD_METHOD:
        write_response(h, RESP_501, sizeof(RESP_501));
        return;
    }
}


// event handlers

/**
 * Handles SIGINT by stopping the default event loop.
 */
static void sigint_cb(struct ev_loop *loop, ev_signal *w, int events) {
    puts("stopping");
    ev_break(loop, EVBREAK_ALL);
}

/**
 * Handles input events from client sockets.
 */
static void read_cb(struct ev_loop *loop, ev_io *w, int events) {
    struct handler *h;

    puts("readable");
    h = (struct handler*) w->data;
    parse_request(h);

    switch (h->state) {
    case ST_ERROR:
        debug("read error");
    case ST_END:
        ev_io_stop(loop, w);
        close(h->fd);
        debug("client disconnected");
    }
}

/**
 * Handles I/O events from server socket.
 */
static void accept_cb(struct ev_loop *loop, ev_io *w, int events) {
    struct ev_io *watcher;
    struct server *server;
    struct handler *h;
    struct timeval t;
    int fd;

    // accept client connection

    server = (struct server*) w->data;
    fd = accept4(server->socket, NULL, NULL, SOCK_NONBLOCK);
    if (fd < 0)
        return;

    debug("client connected");

    // configure new socket

    t.tv_usec = 0;
    t.tv_sec = 30;

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t));

    // configure new handler

    h = get_handler(&(server->handler_pool));
    h->state = ST_START;
    h->fd = fd;

    // configure new watcher

    watcher = &(h->watcher);
    ev_io_init(watcher, read_cb, fd, EV_READ);
    watcher->data = h;

    ev_io_start(loop, watcher);
}


// entry point

int main(int argc, char** argv) {
    struct ev_loop *loop;
    struct ev_signal signal_watcher;
    struct ev_io socket_watcher;
    struct server server;

    debug("enabled verbose output");

    server.socket = open_server_socket(
            (argc > 1) ? argv[1] : DEFAULT_SERVICE);
    init_handler_pool(&server.handler_pool);

    ev_signal_init(&signal_watcher, sigint_cb, SIGINT);
    ev_io_init(&socket_watcher, accept_cb, server.socket, EV_READ);

    socket_watcher.data = &server;
    loop = EV_DEFAULT;

    ev_signal_start(loop, &signal_watcher);
    ev_io_start(loop, &socket_watcher);

    puts("server started");
    ev_run(loop, 0);

    close(server.socket);
    puts("server stopped");
    return 0;
}

