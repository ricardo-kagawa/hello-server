/**
 * Utility definitions.
 */

#ifndef UTIL
#define UTIL

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "config.h"


// data types

struct buffer {
    struct chunk* head;
    struct chunk* tail;
    int size;
    int tsize;
};

struct chunk {
    struct chunk* next;
    char data[BUFFER_SIZE];
};

struct {
    struct chunk *pool;
    int size;
} chunk_pool;


// macros

#define TRUE  1
#define FALSE 0

#ifdef DEBUG
#define debug(...) do { \
    printf(__VA_ARGS__); \
    putchar('\n'); \
} while (0)

#else
#define debug(...) do {} while (0)

#endif

#define min(a, b) ((a) < (b) ? (a) : (b))

// functions

/**
 * Initializes an empty buffer.
 */
void init_buffer(struct buffer*);

/**
 * Clears the contents of the buffer, releasing all chunks.
 */
void clear_buffer(struct buffer*);

int buffer_append(struct buffer*, char[], int);

int buffer_append_char(struct buffer*, char);

char buffer_get(struct buffer*, int);

/**
 * Checks that the buffer (offset by some bytes) contains the given
 * prefix (of some length), case sensitively.
 */
int buffer_starts_with(struct buffer*, int, const char[], int);

/**
 * Checks that the buffer (offset by some bytes) contains the given
 * prefix (of some length), case insensitively.
 */
int buffer_istarts_with(struct buffer*, int, const char[], int);

/**
 * Copies buffer data to a regular character array. The array is
 * null-terminated, so it is a regular C string, and it is allocated
 * dynamically, so it should be freed after use.
 */
char* buffer_copy(struct buffer*, int, int);

int buffer_shift(struct buffer*);

int buffer_write(struct buffer*, int, int);

#ifdef DEBUG
void buffer_debug(struct buffer*);
#else
#define buffer_debug(b) do {} while (0)
#endif

#endif

