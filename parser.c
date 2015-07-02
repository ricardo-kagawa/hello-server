#include "parser.h"

// constants

#define HEADER_OK    0
#define HEADER_STOP  1
#define HEADER_ERROR 2

#define H_BAD_HEADER     0
#define H_GENERIC        1
#define H_CONTENT_LENGTH 2

#define CRLF "\r\n"

const char m_get[] = "GET ";
const char m_head[] = "HEAD ";
const char http_version[] = "HTTP/1.x";

const char h_content_length[] = "Content-Length:";

const unsigned char uri_chars[] = {
//  Control Characters and Spaces (starts at 0x00)
//  0x07        0x0F        0x17        0x1F
    0x00,       0x00,       0x00,       0x00,

//   !"#$%&'    ()*+,-./    01234567    89:;<=>?
//  01011111    11111111    11111111    11110101
    0x5F,       0xFF,       0xFF,       0xF5,

//  @ABCDEFG    HIJKLMNO    PQRSTUVW    XYZ[\]^_
//  11111111    11111111    11111111    11110101
    0xFF,       0xFF,       0xFF,       0xF5,

//  `abcdefg    hijklmno    pqrstuvw    xyz{|}~
//  01111111    11111111    11111111    1110001
    0x7F,       0xFF,       0xFF,       0xE1
};

const unsigned char token_chars[] = {
//  Control Characters and Spaces (starts at 0x00)
//  0x07        0x0F        0x17        0x1F
    0x00,       0x00,       0x00,       0x00,

//   !"#$%&'    ()*+,-./    01234567    89:;<=>?
//  01011111    00110110    11111111    11000000
    0x5F,       0x36,       0xFF,       0xC0,

//  @ABCDEFG    HIJKLMNO    PQRSTUVW    XYZ[\]^_
//  01111111    11111111    11111111    11100011
    0x7F,       0xFF,       0xFF,       0xE3,

//  `abcdefg    hijklmno    pqrstuvw    xyz{|}~
//  11111111    11111111    11111111    1110101
    0xFF,       0xFF,       0xFF,       0xE5
};


// macros

#define ready(p) ((p)->buffer.size - (p)->mark)


// functions

#include <string.h>

/**
 * Reads data from the socket to the internal buffer.
 */
int read_socket(struct parser *p) {
    struct chunk *c;
    char buffer[BUFFER_SIZE];
    int n, t;

    debug("reading socket");

    t = 0;
    n = read(p->fd, buffer, BUFFER_SIZE);
    while (n > 0) {
        t += n;
        buffer_append(&(p->buffer), buffer, n);
        n = read(p->fd, buffer, BUFFER_SIZE);
    }

    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        p->state = PARSING_ERROR;
        p->error = E_READ;
        return FALSE;
    }

    debug("%d bytes read", t);
    return TRUE;
}

void advance_mark(struct parser *p, int n) {
    p->mark += n;
    if (p->mark > BUFFER_SIZE)
        p->mark -= buffer_shift(&(p->buffer));
}

/**
 * Checks if given character is a valid HTTP token character.
 */
int is_token_char(char c) {
    int i = (unsigned) c >> 3;
    int j = (unsigned) 0x80 >> (c & 0x07);
    return (token_chars[i] & j) != 0;
}

/**
 * Checks if given character is a valid URI character.
 */
int is_uri_char(char c) {
    int i = (unsigned) c >> 3;
    int j = (unsigned) 0x80 >> (c & 0x07);
    return (uri_chars[i] & j) != 0;
}


/**
 * Parses a known token value from the reader. Returns TRUE when done,
 * FALSE otherwise. The parser state is set to PARSING_ERROR on failure,
 * and is not changed otherwise.
 */
int parse_constant(struct parser *p, const char token[], int n) {
    if (ready(p) < n)
        return PARSING_WAIT;

    if (!buffer_starts_with(&(p->buffer), p->mark, token, n))
        return PARSING_ERROR;

    advance_mark(p, n);
    return PARSING_DONE;
}

/**
 * Parses the request method.
 */
int parse_request_method(struct parser *p) {
    char first;
    int r;

    if (ready(p) == 0)
        return PARSING_WAIT;

    first = buffer_get(&(p->buffer), p->mark);
    if (first == m_get[0]) {
        r = parse_constant(p, m_get, sizeof(m_get) - 1);
        if (r != PARSING_DONE)
            return r;

        p->request.method = METHOD_GET;
    } else if (first == m_head[0]) {
        r = parse_constant(p, m_head, sizeof(m_head) - 1);
        if (r != PARSING_DONE)
            return r;

        p->request.method = METHOD_HEAD;
    } else if (is_token_char(first)) {
        // other methods

        if (ready(p) < 2)
            return PARSING_WAIT;
        advance_mark(p, 1);

        while (is_token_char(buffer_get(&(p->buffer), p->mark))) {
            if (ready(p) < 2)
                return PARSING_WAIT;
            advance_mark(p, 1);
        }

        if (buffer_get(&(p->buffer), p->mark) != ' ')
            return PARSING_ERROR;
        advance_mark(p, 1);

        p->request.method = METHOD_OTHER;
    } else
        return PARSING_ERROR;

    return PARSING_DONE;
}

/**
 * Parses the request URI.
 */
int parse_request_uri(struct parser *p) {
    if (ready(p) < 2)
        return PARSING_WAIT;

    if (!is_uri_char(buffer_get(&(p->buffer), p->mark)))
        return PARSING_ERROR;
    advance_mark(p, 1);

    while (is_uri_char(buffer_get(&(p->buffer), p->mark))) {
        if (ready(p) < 2)
            return PARSING_WAIT;
        advance_mark(p, 1);
    }

    if (buffer_get(&(p->buffer), p->mark) != ' ')
        return PARSING_ERROR;
    advance_mark(p, 1);

    return PARSING_DONE;
}

/**
 * Parses the HTTP version. Must be "HTTP/1.x".
 */
int parse_http_version(struct parser *p) {
    char c;
    int r;

    if (ready(p) < sizeof(http_version) + 1)
        return PARSING_WAIT;

    r = parse_constant(p, http_version, sizeof(http_version) - 2);
    if (r != PARSING_DONE)
        return r;

    c = buffer_get(&(p->buffer), p->mark);
    if (!isdigit(c))
        return PARSING_ERROR;
    p->request.version = c;
    advance_mark(p, 1);

    return parse_constant(p, CRLF, 2);
}

/**
 * Parses an HTTP header name. Some headers are treated especially, as
 * Content-Length, while others are ignored.
 */
int parse_header_name(struct parser *p) {
    int n, r;

    n = sizeof(h_content_length) - 1;
    if (ready(p) < n)
        return PARSING_WAIT;

    // Note: If you need to check for other header names,
    // implement an actual FSM instead of comparing to value

    if (p->state != PARSING_HEADER_NAME_ANY
            && p->request.method == METHOD_OTHER && buffer_istarts_with(
            &(p->buffer), p->mark, h_content_length, n)) {
        // Content-Length
        p->state = PARSING_HEADER_CONTENT_LENGTH;
        advance_mark(p, n);
        return PARSING_DONE;
    }

    // other headers

    p->state = PARSING_HEADER_NAME_ANY;
    if (ready(p) < 2)
        return PARSING_WAIT;

    if (!is_token_char(buffer_get(&(p->buffer), p->mark)))
        return PARSING_ERROR;
    advance_mark(p, 1);

    while (is_token_char(buffer_get(&(p->buffer), p->mark))) {
        if (ready(p) < 2)
            return PARSING_WAIT;
        advance_mark(p, 1);
    }

    if (buffer_get(&(p->buffer), p->mark) != ':')
        return PARSING_ERROR;
    advance_mark(p, 1);

    p->state = PARSING_HEADER_VALUE;
    return PARSING_DONE;
}

/**
 * Parses a header value. Deprecated header line folding is not supported.
 */
int parse_header_value(struct parser *p) {
    char c;

    if (ready(p) < 3)
        return PARSING_WAIT;
    c = buffer_get(&(p->buffer), p->mark);

    while (isgraph(c) || c == ' ' || c == '\t') {
        advance_mark(p, 1);
        if (ready(p) < 3)
            return PARSING_WAIT;
        c = buffer_get(&(p->buffer), p->mark);
    }

    return parse_constant(p, CRLF, 2);
}

/**
 * Parses a Content-Length header value.
 */
int parse_header_content_length(struct parser *p) {
    char c, *buffer;
    int i, m, r;

    if (ready(p) < 3)
        return PARSING_WAIT;
    c = buffer_get(&(p->buffer), p->mark);

    for (m = 1; isdigit(c) || c == ' ' || c == '\t'; m++) {
        if (ready(p) < 3 + m)
            return PARSING_WAIT;
        c = buffer_get(&(p->buffer), p->mark + m);
    }

    buffer = buffer_copy(&(p->buffer), p->mark, m - 1);
    if (buffer == NULL) {
        p->error = E_MEMORY;
        return PARSING_ERROR;
    }
    advance_mark(p, m - 1);

    r = parse_constant(p, CRLF, 2);
    if (r != PARSING_DONE) {
        free(buffer);
        return r;
    }

    errno = 0;
    p->request.content_length = strtol(buffer, NULL, 10);
    free(buffer);
    return (errno == 0) ? PARSING_DONE : PARSING_ERROR;
}

/**
 * Parses HTTP headers. Only a few headers are actually processed, while most
 * values are discarded.
 */
int parse_headers(struct parser *p) {
    int r;

    while (TRUE) {
        switch (p->state) {
        case PARSING_HEADERS:
            r = parse_constant(p, CRLF, 2);
            if (r != PARSING_ERROR)
                return r;
            p->state = PARSING_HEADER_NAME;
            debug("parsing header");

        case PARSING_HEADER_NAME:
        case PARSING_HEADER_NAME_ANY:
            r = parse_header_name(p);
            if (r != PARSING_DONE)
                return r;
            debug("parsed header name");
        }

        switch (p->state) {
        case PARSING_HEADER_VALUE:
            r = parse_header_value(p);
            break;

        case PARSING_HEADER_CONTENT_LENGTH:
            r = parse_header_content_length(p);
            break;

        default:
            return PARSING_ERROR;
        }

        if (r != PARSING_DONE)
            return r;
        p->state = PARSING_HEADERS;
        debug("parsed header value");
    }
    return PARSING_DONE;
}

/**
 * Reads the request body.
 */
int parse_body(struct parser *p) {
    int n;

    if (p->request.content_length == 0)
        return PARSING_DONE;

    n = ready(p);
    if (n == 0)
        return PARSING_WAIT;

    p->body += n;
    advance_mark(p, n);
    return (p->body < p->request.content_length) ?
            PARSING_WAIT : PARSING_DONE;
}

/**
 * Parses the client request. The request is parsed to properly consume
 * the request data, identifying GET and HEAD HTTP methods. Most of the
 * data read is actually discarded, in the current implementation.
 */
void parse_request(struct parser *p) {
    int r;

    if(!read_socket(p))
        return;

    switch (p->state) {
    case PARSING_START:
        p->mark = 0;
        p->state = PARSING_METHOD;
        debug("parse started");

    case PARSING_METHOD:
        r = parse_request_method(p);
        if (r != PARSING_DONE)
            break;
        p->state = PARSING_URI;
        debug("parsed method: %d", p->request.method);

    case PARSING_URI:
        r = parse_request_uri(p);
        if (r != PARSING_DONE)
            break;
        p->state = PARSING_VERSION;
        debug("parsed uri");

    case PARSING_VERSION:
        r = parse_http_version(p);
        if (r != PARSING_DONE)
            break;
        p->state = PARSING_HEADERS;
        debug("parsed http version");

    case PARSING_HEADERS:
    case PARSING_HEADER_NAME:
    case PARSING_HEADER_NAME_ANY:
    case PARSING_HEADER_VALUE:
    case PARSING_HEADER_CONTENT_LENGTH:
        r = parse_headers(p);
        if (r != PARSING_DONE)
            break;
        p->state = PARSING_BODY;
        debug("parsed headers");
        debug("content-length: %ld", p->request.content_length);

    case PARSING_BODY:
        r = parse_body(p);
        if (r != PARSING_DONE)
            break;
        p->state = PARSING_DONE;
        debug("parsed body");
        return;

    default:
        debug("illegal parser state: %d", p->state);
        p->state = PARSING_ERROR;
        p->error = E_PARSE;
        return;
    }

    if (r == PARSING_ERROR) {
        p->state = PARSING_ERROR;
        if (p->error == E_NONE)
            p->error = E_PARSE;
    }
    return;
}

void init_parser(struct parser* p) {
    p->state = PARSING_METHOD;
    p->error = E_NONE;

    p->fd = -1;
    p->mark = 0;
    p->body = 0;
    init_buffer(&(p->buffer));

    p->request.version = '0';
    p->request.content_length = 0;
}

void free_parser(struct parser *p) {
    clear_buffer(&(p->buffer));
}

#undef ensure_data

