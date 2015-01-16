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


/**
 * Server state.
 */
struct server_t {
    int socket;
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
    int fd, val;

    debug("service: %s", service);
    ai = get_server_addrinfo(service);

    aip = ai;
    while (aip != NULL) {
        fd = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);

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
 * Handles SIGINT by stopping the default event loop.
 */
static void sigint_cb(struct ev_loop *loop, ev_signal *w, int events) {
    puts("stopping");
    ev_break(loop, EVBREAK_ALL);
}

// see header file
int read_socket(struct request *req) {
    debug("reading socket");
    req->size = read(req->fd, req->data, sizeof(req->data));
    req->mark = 0;

    debug("%d bytes read", req->size);
    return (req->size > 0);
}

/**
 * Writes the given response with size bytes to the socket pointed to by req.
 * The corresponding HTTP minor version in req is written to the response.
 */
void write_response(struct request *req, char resp[], int size) {
    write(req->fd, HTTP_VERSION, sizeof(HTTP_VERSION) - 1);
    write(req->fd, &(req->version), sizeof(char));
    write(req->fd, resp, size - 1);
}

/**
 * Handles a new client connection.
 */
void handle_connection(int socket) {
    struct request req;
    struct timeval t;

    debug("client connected");

    t.tv_usec = 0;
    t.tv_sec = 30;

    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t));

    req.fd = socket;
    req.size = 0;
    req.mark = 0;

    switch (parse_request(&req)) {
    case REQ_GET:
        write_response(&req, RESP_200, sizeof(RESP_200));
        return;

    case REQ_HEAD:
        write_response(&req, RESP_200H, sizeof(RESP_200H));
        return;

    case REQ_BAD_HTTP:
        write_response(&req, RESP_400, sizeof(RESP_400));
        return;

    case REQ_BAD_METHOD:
        write_response(&req, RESP_501, sizeof(RESP_501));
        return;
    }
}

/**
 * Handles I/O events from server socket.
 */
static void accept_cb(struct ev_loop *loop, ev_io *w, int events) {
    struct server_t *server;
    int fd;

    server = (struct server_t*) w->data;

    fd = accept(server->socket, NULL, NULL);
    if (fd < 0)
        return;

    handle_connection(fd);
    close(fd);
}

int main(int argc, char** argv) {
    struct ev_loop *loop;
    struct ev_signal signal_watcher;
    struct ev_io socket_watcher;
    struct server_t server;

    debug("enabled verbose output");

    server.socket = open_server_socket(
            (argc > 1) ? argv[1] : DEFAULT_SERVICE);

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

