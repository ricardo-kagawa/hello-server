/**
 * HTTP parser.
 */

#ifndef PARSER
#define PARSER

#include <errno.h>

#include "config.h"
#include "errors.h"
#include "util.h"


// constants

#define METHOD_OTHER    0
#define METHOD_HEAD     1
#define METHOD_GET      2

#define PARSING_START           0
#define PARSING_WAIT            1
#define PARSING_DONE            2
#define PARSING_ERROR           3
#define PARSING_METHOD          4
#define PARSING_URI             5
#define PARSING_VERSION         6
#define PARSING_HEADERS         7
#define PARSING_HEADER_NAME     8
#define PARSING_HEADER_VALUE    9
#define PARSING_BODY            10

#define PARSING_HEADER_NAME_ANY         20
#define PARSING_HEADER_CONTENT_LENGTH   21


// data types

struct request {
    int method;
    char version;
    long content_length;
};

struct parser {
    int state;
    int error;
    int fd;
    int mark;
    int body;
    struct buffer buffer;
    struct request request;
};


// functions

void init_parser(struct parser*);

void free_parser(struct parser*);

void parse_request(struct parser*);

#endif

