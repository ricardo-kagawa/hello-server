#include "server.h"

#define REQ_BAD_HTTP   0
#define REQ_GET        1
#define REQ_HEAD       2
#define REQ_BAD_METHOD 3

#define HEADER_OK    0
#define HEADER_STOP  1
#define HEADER_ERROR 2

#define H_BAD_HEADER     0
#define H_GENERIC        1
#define H_CONTENT_LENGTH 2

#define CRLF "\r\n"


// constants

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

#define min(a, b) ((a) < (b) ? (a) : (b))

// If the buffered data is consumed, attempt to read more data
// from the socket. If that fails, return the error code.
#define ensure_data(req, ret) do { \
    if (req->size - req->mark == 0 && !read_socket(req)) \
        return ret; \
} while (0)


// functions

/**
 * Reads a known token value from the request. Returns zero on failure (if
 * there was not enough data to read, or if the available data did not match
 * the expected token).
 */
int read_constant(struct request *req, const char token[], int n) {
    int m, p;

    for (p = 0; p < n; p += m) {
        ensure_data(req, FALSE);

        m = min(req->size - req->mark, n - p);
        if (memcmp(req->data + req->mark, token + p, m) != 0)
            return FALSE;

        req->mark += m;
    }

    return TRUE;
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
 * Parses the request method. Returns REQ_GET or REQ_HEAD on success,
 * or REQ_BAD_HTTP or REQ_BAD_METHOD on error.
 */
int parse_request_method(struct request *req) {
    ensure_data(req, REQ_BAD_HTTP);

    if (req->data[0] == m_get[0]) {
        req->mark = 1;
        return read_constant(req, m_get + 1, sizeof(m_get) - 2) ?
                REQ_GET : REQ_BAD_METHOD;
    } else if (req->data[0] == m_head[0]) {
        req->mark = 1;
        return read_constant(req, m_head + 1, sizeof(m_head) - 2) ?
                REQ_HEAD : REQ_BAD_METHOD;
    } else {
        req->mark = 1;

        ensure_data(req, REQ_BAD_HTTP);
        while (is_token_char(req->data[req->mark])) {
            req->mark++;
            ensure_data(req, REQ_BAD_HTTP);
        }

        if (req->data[req->mark] == ' ') {
            req->mark++;
            return REQ_BAD_METHOD;
        } else
            return REQ_BAD_HTTP;
    }
}

/**
 * Parses the request URI. Returns zero on failure (if the request does not
 * contain a string with valid URI characters as the URI value).
 */
int parse_request_uri(struct request *req) {
    ensure_data(req, FALSE);
    while (is_uri_char(req->data[req->mark])) {
        req->mark++;
        ensure_data(req, FALSE);
    }

    if (req->data[req->mark] != ' ')
        return FALSE;

    req->mark++;
    return TRUE;
}

/**
 * Parses the HTTP version. Must be 1.x. Returns zero on failure.
 */
int parse_http_version(struct request *req) {
    if (!read_constant(req, http_version, sizeof(http_version) - 2))
        return FALSE;

    ensure_data(req, FALSE);
    if (isdigit(req->data[req->mark])) {
        req->version = req->data[req->mark];
        req->mark++;
        read_constant(req, CRLF, 2);
        return TRUE;
    } else
        return FALSE;
}

/**
 * Parses an HTTP header name. It is treated especially to locate specific
 * headers, like Content-Length. Return zero on failure.
 */
int parse_header_name(struct request *req) {
    int m, n, p;
    char *data;

    // Note: If you need to check for other header names,
    // implement an actual FSM instead of comparing to value

    m = sizeof(h_content_length) - 1;
    for (p = 0; p < m; p += n) {
        ensure_data(req, FALSE);
        data = req->data + req->mark;
        n = min(req->size - req->mark, m - p);

        if (strncasecmp(data, h_content_length + p, n) != 0)
            break;

        req->mark += n;
    }

    // Content-Length
    if (p == m)
        return H_CONTENT_LENGTH;

    // other headers

    ensure_data(req, HEADER_ERROR);
    while (is_token_char(req->data[req->mark])) {
        req->mark++;
        ensure_data(req, HEADER_ERROR);
    }

    if (req->data[req->mark] != ':')
        return HEADER_ERROR;

    req->mark++;
    return H_GENERIC;
}

/**
 * Parses a header value. Deprecated header line folding is not supported.
 */
int parse_header_value(int header, struct request *req) {
    char buffer[BUFFER_SIZE], c, p;

    p = 0;
    ensure_data(req, FALSE);
    c = req->data[req->mark];

    while (isgraph(c) || c == ' ' || c == '\t') {
        if (header != H_GENERIC)
            buffer[p++] = c;

        req->mark++;
        ensure_data(req, FALSE);
        c = req->data[req->mark];
    }

    if (read_constant(req, CRLF, 2)) {
        switch (header) {
        case H_CONTENT_LENGTH:
            buffer[p] = '\0';

            errno = 0;
            req->content_length = strtol(buffer, NULL, 10);
            if (errno != 0)
                req->content_length = 0;

            return TRUE;

        default:
            return TRUE;
        }
    } else
        return FALSE;
}

/**
 * Parses a single HTTP header. Most headers will be ignored, except
 * Content-Length for proper request consumption.
 */
int parse_header(struct request *req) {
    int h;

    if (read_constant(req, CRLF, 2))
        return HEADER_STOP;

    h = parse_header_name(req);
    if (h == H_BAD_HEADER)
        return HEADER_ERROR;

    debug("header: %d", h);

    if (!parse_header_value(h, req))
        return HEADER_ERROR;

    return HEADER_OK;
}

/**
 * Parses HTTP headers. Only a few headers are actually processed, while most
 * values are discarded. Returns zero on failure (bad header syntax or socket
 * error).
 */
int parse_headers(struct request *req) {
    int r;

    req->content_length = 0;

    r = parse_header(req);
    while (r == HEADER_OK)
        r = parse_header(req);

    return (r == HEADER_STOP) ? TRUE : FALSE;
}

/**
 * Reads request body.
 */
int read_body(struct request *req) {
    int m, n;

    for (n = 0; n < req->content_length; n += m) {
        ensure_data(req, FALSE);
        m = min(req->size - req->mark, req->content_length - n);
        req->mark += m;
    }
    return TRUE;
}

/**
 * Parses the client request. The request is parsed to properly consume
 * the request data, identifying GET and HEAD HTTP methods. Most of the
 * data read is actually discarded, in the current implementation.
 */
int parse_request(struct request *req) {
    int method;

    method = parse_request_method(req);
    if (method == REQ_BAD_HTTP)
        return method;
    debug("parsed method: %d", method);

    if (!parse_request_uri(req))
        return REQ_BAD_HTTP;
    debug("parsed URI");

    if (!parse_http_version(req))
        return REQ_BAD_HTTP;
    debug("parsed version: %c", req->version);

    if (!parse_headers(req))
        return REQ_BAD_HTTP;
    debug("parsed headers");

    debug("content-length: %ld", req->content_length);


    if (!read_body(req))
        return REQ_BAD_HTTP;

    if (req->content_length > 0)
        debug("consumed body");

    return method;
}

#undef ensure_data

