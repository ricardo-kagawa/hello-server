#include "server.h"

#define HTTP_VERSION "HTTP/1."
#define CONTENT_LENGTH "Content-Length: "
#define RESP_200 " 200 OK\r\n"
#define RESP_400 " 400 Bad Request\r\n"
#define RESP_500 " 500 Internal Server Error\r\n"
#define RESP_501 " 501 Not Implemented\r\n"


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
 * Allocates a handler. If none could be, NULL is returned.
 */
struct handler* new_handler(struct server *server) {
    struct handler *h;

    if (server->handler_pool == NULL) {
        if (server->handler_count >= MAX_HANDLERS)
            return NULL;

        h = (struct handler*) malloc(sizeof(struct handler));
        if (h == NULL)
            return NULL;

        server->handler_count++;
        debug("new request handler: %p", h);
    } else {
        h = server->handler_pool;
        server->handler_pool = h->next;
        debug("old request handler: %p", h);
    }

    h->next = NULL;
    h->pool = server;
    init_parser(&(h->parser));
    init_buffer(&(h->response.data));
    h->response.mark = 0;
    return h;
}

void free_handler(struct handler* h) {
    h->next = h->pool->handler_pool;
    h->pool->handler_pool = h;
    clear_buffer(&(h->response.data));
    debug("handler returned: %p", h);
}

int build_response(struct handler *h) {
    struct buffer *resp;
    struct parser *p;
    int r;

    p = &(h->parser);
    resp = &(h->response.data);


    // protocol and version

    r = buffer_append(resp, HTTP_VERSION, sizeof(HTTP_VERSION) - 1);
    r = r && buffer_append_char(resp, p->request.version);


    // status line

    switch (p->state) {
    case PARSING_ERROR:
        error(p->error, 0);
        h->error = p->error;
        switch (p->error) {
        case E_READ:
        case E_PARSE:
            r = r && buffer_append(resp, RESP_400, sizeof(RESP_400) - 1);
            break;

        case E_MEMORY:
            r = r && buffer_append(resp, RESP_500, sizeof(RESP_500) - 1);
            break;
        }
        break;

    case PARSING_DONE:
        switch (p->request.method) {
        case METHOD_GET:
        case METHOD_HEAD:
            r = r && buffer_append(resp, RESP_200, sizeof(RESP_200) - 1);
            break;

        default:
            r = r && buffer_append(resp, RESP_501, sizeof(RESP_501) - 1);
            break;
        }
    }


    // content-length

    r = r && buffer_append(resp, CONTENT_LENGTH, sizeof(CONTENT_LENGTH) - 1);
    if (p->state == PARSING_DONE && p->request.method != METHOD_OTHER) {
        r = r && buffer_append(resp, "11\r\n", 4);
    } else
        r = r && buffer_append(resp, "0\r\n", 3);


    // response body

    r = r && buffer_append(resp, "\r\n", 2);
    if (p->state == PARSING_DONE && p->request.method == METHOD_GET) {
        r = r && buffer_append(resp, "hello world", 11);
    }


    // footer

    if (!r) {
        h->state = ST_ERROR;
        h->error = E_MEMORY;
        return FALSE;
    } else {
        debug("response built");
        return TRUE;
    }
}


// event handlers

/**
 * Handles SIGINT by stopping the default event loop.
 */
static void sigint_cb(struct ev_loop *loop, ev_signal *w, int events) {
    struct server* server;
    struct handler *h;

    puts("stopping");
    ev_break(loop, EVBREAK_ALL);
}

static void write_cb(struct ev_loop *loop, ev_io *w, int events) {
    struct handler *h;
    struct buffer *b;
    int n;

    h = (struct handler*) w->data;
    b = &(h->response.data);

    h->response.mark += buffer_write(b, h->response.mark, h->fd);
    if (errno == 0 && b->size - h->response.mark > 0) {
        return;
    } else if (errno != 0) {
        error(E_WRITE, errno);
    } else
        debug("response written");

    ev_io_stop(loop, w);
    close(h->fd);
    debug("client disconnected");
    free_handler(h);
}

/**
 * Handles input events from client sockets.
 */
static void read_cb(struct ev_loop *loop, ev_io *w, int events) {
    struct handler *h;
    struct parser *p;

    h = (struct handler*) w->data;
    p = &(h->parser);

    parse_request(p);
    if (p->state == PARSING_WAIT) {
        return;
    }

    ev_io_stop(loop, w);
    if (p->state == PARSING_ERROR && p->error == E_MEMORY) {
        free_parser(&(h->parser));
        close(h->fd);

        error(p->error, 0);
        debug("client disconnected");
        free_handler(h);
        return;
    }

    debug("request processed");

    if (!build_response(h)) {
        free_parser(&(h->parser));
        close(h->fd);

        error(h->error, 0);
        debug("client disconnected");
        free_handler(h);
        return;
    }

    h->state = ST_WRITING;
    free_parser(&(h->parser));

    ev_io_init(w, write_cb, h->fd, EV_WRITE);
    w->data = h;
    ev_io_start(loop, w);
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

    // check handler pool

    server = (struct server*) w->data;
    h = new_handler(server);
    if (h == NULL) {
        debug("out of handlers");
        error(E_MEMORY, 0);
        return;
    }

    // accept client connection

    fd = accept4(server->socket, NULL, NULL, SOCK_NONBLOCK);
    if (fd < 0) {
        free_handler(h);
        return;
    }

    debug("client connected");

    // configure new socket

    t.tv_usec = 0;
    t.tv_sec = 30;

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t));

    // configure new handler

    h->state = ST_READING;
    h->error = E_NONE;
    h->parser.fd = fd;
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
    server.handler_pool = NULL;
    server.handler_count = 0;

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

