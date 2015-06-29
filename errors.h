// general errors

#ifndef ERRORS
#define ERRORS

#include <errno.h>
#include <stdio.h>
#include <netdb.h>


// general errors

#define E_NONE      0
#define E_MEMORY    1

// setup errors

#define E_ADDRINFO  2
#define E_SOCKET    3
#define E_BIND      4
#define E_LISTEN    5

// processing errors

#define E_READ      6
#define E_PARSE     7


/**
 * Outputs a corresponding error message to stderr.
 */
void error(int cat, int code);

#endif

