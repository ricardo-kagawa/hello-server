#include <errno.h>
#include "util.h"


// see header file
void init_buffer(struct buffer *b) {
    b->head = NULL;
    b->tail = NULL;
    b->size = 0;
    b->tsize = 0;
}

void clear_buffer(struct buffer *b) {
    struct chunk *c;

    b->tail = NULL;
    while (b->head != NULL) {
        c = b->head;
        b->head = b->head->next;

        if (chunk_pool.size < MAX_BUFFERS) {
            c->next = chunk_pool.pool;
            chunk_pool.pool = c;
            chunk_pool.size++;
        } else
            free(c);
    }
}

struct chunk* new_chunk() {
    struct chunk *p;

    if (chunk_pool.pool != NULL) {
        p = chunk_pool.pool;
        chunk_pool.pool = p->next;
        chunk_pool.size--;
    } else
        p = (struct chunk*) malloc(sizeof(struct chunk));

    p->next = NULL;
    return p;
}

int buffer_append(struct buffer *b, char data[], int n) {
    int m, p;

    if (b->head == NULL) {
        b->tail = new_chunk();
        if (b->tail == NULL)
            return FALSE;
        b->head = b->tail;
    }

    p = 0;
    m = BUFFER_SIZE - b->tsize;
    while (n - p > m) {
        memcpy(b->tail->data + b->tsize, data + p, m);
        p += m;

        m = BUFFER_SIZE;
        b->tail->next = new_chunk();
        if (b->tail->next == NULL)
            return FALSE;

        b->tail = b->tail->next;
        b->tsize = 0;
    }

    memcpy(b->tail->data + b->tsize, data + p, n - p);
    b->tsize += n - p;
    b->size += n;
    return TRUE;
}

int buffer_append_char(struct buffer *b, char c) {
    if (b->head == NULL) {
        b->tail = new_chunk();
        if (b->tail == NULL)
            return FALSE;
        b->head = b->tail;
    } else if (BUFFER_SIZE - b->tsize == 0) {
        b->tail->next = new_chunk();
        if (b->tail->next == NULL)
            return FALSE;
        b->tail = b->tail->next;
        b->tsize = 0;
    }

    b->tail->data[b->tsize] = c;
    b->tsize++;
    b->size++;
    return TRUE;
}

char buffer_get(struct buffer *b, int index) {
    struct chunk *c;
    int i, n;

    if (index >= b->size)
        return -1;

    c = b->head;
    n = index / BUFFER_SIZE;
    for (i = 0; i < n; i ++)
        c = c->next;
    return c->data[index % BUFFER_SIZE];
}

int buffer_starts_with(struct buffer *b, int p, const char data[], int n) {
    struct chunk *c;
    int i, k, k0, k1, m;

    if (b->size - p < n)
        return FALSE;

    if (n == 0)
        return TRUE;

    k0 = p / BUFFER_SIZE;
    k1 = (p + n) / BUFFER_SIZE;

    // skip offset

    c = b->head;
    for (i = 0; i < k0; i++)
        c = c->next;

    if (k0 == k1) {
        // everything is in a single chunk
        k = p % BUFFER_SIZE;
        return (memcmp(c->data + k, data, n) == 0);
    }

    // compare end of first chunk

    k = p % BUFFER_SIZE;
    if (memcmp(c->data + k, data, BUFFER_SIZE - k) != 0)
        return FALSE;
    c = c->next;

    // intermediate chunks

    for (m = k; n - m > BUFFER_SIZE; m += BUFFER_SIZE) {
        if (memcmp(c->data, data + m, BUFFER_SIZE) != 0)
            return FALSE;
        c = c->next;
    }

    // start of last chunk

    return (memcmp(c->data, data + m, n - m) == 0);
}

int buffer_istarts_with(struct buffer *b, int p, const char data[], int n) {
    struct chunk *c;
    int i, k, k0, k1, m;

    if (b->size - p < n)
        return FALSE;

    if (n == 0)
        return TRUE;

    k0 = p / BUFFER_SIZE;
    k1 = (p + n) / BUFFER_SIZE;

    // skip offset

    c = b->head;
    for (i = 0; i < k0; i++)
        c = c->next;

    if (k0 == k1) {
        // everything is in a single chunk
        k = p % BUFFER_SIZE;
        return (strncasecmp(c->data + k, data, n) == 0);
    }

    // compare end of first chunk

    k = p % BUFFER_SIZE;
    if (strncasecmp(c->data + k, data, BUFFER_SIZE - k) != 0)
        return FALSE;
    c = c->next;

    // intermediate chunks

    for (m = k; n - m > BUFFER_SIZE; m += BUFFER_SIZE) {
        if (strncasecmp(c->data, data + m, BUFFER_SIZE) != 0)
            return FALSE;
        c = c->next;
    }

    // start of last chunk

    return (strncasecmp(c->data, data + m, n - m) == 0);
}

char* buffer_copy(struct buffer *b, int p, int n) {
    struct chunk *c;
    int i, k, k0, k1, m;
    char* data;

    if (b->size - p < n)
        return NULL;

    data = (char*) malloc((n + 1) * sizeof(char));
    if (data == NULL)
        return NULL;

    data[n] = '\0';
    if (n == 0)
        return data;

    k0 = p / BUFFER_SIZE;
    k1 = (p + n) / BUFFER_SIZE;

    // skip offset

    c = b->head;
    for (i = 0; i < k0; i++)
        c = c->next;

    if (k0 == k1) {
        // everything is in a single chunk
        k = p % BUFFER_SIZE;
        memcpy(data, c->data + k, n);
        return data;
    }

    // copy end of first chunk

    k = p % BUFFER_SIZE;
    memcpy(data, c->data + k, BUFFER_SIZE - k);
    c = c->next;

    // intermediate chunks

    for (m = k; n - m > BUFFER_SIZE; m += BUFFER_SIZE) {
        memcpy(data + m, c->data, BUFFER_SIZE);
        c = c->next;
    }

    // start of last chunk

    memcpy(data + m, c->data, n - m);
    return data;
}

int buffer_shift(struct buffer *b) {
    struct chunk *c;
    int r;

    if (b->head == NULL)
        return 0;

    if (b->head == b->tail) {
        r = b->size;
        free(b->head);
        b->head = NULL;
        b->tail = NULL;
        b->size = 0;
        b->tsize = 0;
    } else {
        r = BUFFER_SIZE;
        c = b->head;
        b->head = c->next;
        b->size -= BUFFER_SIZE;
        free(c);
    }
    return r;
}

int buffer_write(struct buffer *b, int p, int fd) {
    struct chunk *c;
    int i, k, k0, k1, m, r;
    char* data;

    errno = 0;
    if (b->size - p  == 0)
        return TRUE;

    k0 = p / BUFFER_SIZE;
    k1 = (p + b->size) / BUFFER_SIZE;

    // skip offset

    c = b->head;
    for (i = 0; i < k0; i++)
        c = c->next;

    if (k0 == k1) {
        // everything is in a single chunk
        r = write(fd, c->data + p % BUFFER_SIZE, b->size);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            errno = 0;
        return r;
    }

    // copy end of first chunk

    k = p % BUFFER_SIZE;
    r = write(fd, c->data + k, BUFFER_SIZE - k);
    if (errno != 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            errno = 0;
        return r;
    }
    c = c->next;

    // intermediate chunks

    for (m = k; b->size - m > BUFFER_SIZE; m += BUFFER_SIZE) {
        r = write(fd, c->data, BUFFER_SIZE);
        if (errno != 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                errno = 0;
            return r;
        }
        c = c->next;
    }

    // start of last chunk

    r = write(fd, c->data, b->size - m);
    if (errno == EAGAIN || errno == EWOULDBLOCK)
        errno = 0;
    return r;
}

#ifdef DEBUG
void buffer_debug(struct buffer *b) {
    struct chunk* p;
    int i, n;
    char c;

    printf("size=%d,data=[", b->size);
    for (p = b->head; p != NULL; p = p->next) {
        n = (p->next == NULL) ? b->tsize : BUFFER_SIZE;
        for (i = 0; i < n; i++) {
            c = p->data[i];
            if (!isprint(c)) {
                putchar('\\');
                switch(c) {
                case '\r':
                    putchar('r');
                    break;
                case '\n':
                    putchar('n');
                    putchar('\n');
                    break;
                default:
                    putchar('?');
                }
            } else
                putchar(c);
        }
    }
    putchar(']');
    putchar('\n');
}
#endif

