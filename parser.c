#include "server.h"

#define REQ_BAD_HTTP   0
#define REQ_GET        1
#define REQ_HEAD       2
#define REQ_BAD_METHOD 3


// constants

const char m_get[] = "GET ";
const char m_head[] = "HEAD ";

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


/**
 * Reads a known token value from the request. Returns zero on failure (if
 * there was not enough data to read, or if the available data did not match
 * the expected token).
 */
int read_token(struct request *req, const char token[], int n) {
    int m, p;

    for (p = 0; p < n; p += m) {
        m = min(req->size - req->mark, n - p);
        if (memcmp(req->data + req->mark, token + p, m) != 0)
            return FALSE;

        req->mark += m;
        if (req->size - req->mark == 0 && !read_socket(req))
            return FALSE;
    }

    return TRUE;
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
    if (req->data[0] == m_get[0]) {
        req->mark = 1;
        return read_token(req, m_get + 1, sizeof(m_get) - 2) ?
                REQ_GET : REQ_BAD_METHOD;
    } else if (req->data[0] == m_head[0]) {
        req->mark = 1;
        return read_token(req, m_head + 1, sizeof(m_head) - 2) ?
                REQ_HEAD : REQ_BAD_METHOD;
    } else
        return REQ_BAD_METHOD;
}

/**
 * Parses the request URI. Returns zero on failure (if the request does not
 * contain a string with valid URI characters as the URI value).
 */
int parse_request_uri(struct request *req) {
    int p; char c;

    if (req->size - req->mark == 0 && !read_socket(req))
        return FALSE;

    c = req->data[req->mark];
    while (is_uri_char(req->data[req->mark])) {
        req->mark++;

        if (req->size - req->mark == 0 && !read_socket(req))
            return FALSE;

        c = req->data[req->mark];
    }

    if (req->data[req->mark] != ' ')
        return FALSE;

    req->mark++;
    return TRUE;
}

/**
 * Parses the client request. The request is parsed to properly consume
 * the request data, identifying GET and HEAD HTTP methods. Most of the
 * data read is actually discarded, in the current implementation.
 */
int parse_request(struct request *req) {
    int method;

    if (req->size == 0 && !read_socket(req))
        return REQ_BAD_HTTP;

    method = parse_request_method(req);
    if (method == REQ_BAD_HTTP)
        return method;

    if (!parse_request_uri(req))
        return REQ_BAD_HTTP;

    return method;
}

