#include "errors.h"

void error(int err, int code) {
    fputs("ERROR ", stderr);

    switch (err) {
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

    case E_MEMORY:
        fputs("Out of memory", stderr);
        break;

    case E_READ:
        fputs("Could not read socket", stderr);
        break;

    case E_PARSE:
        fputs("Could not parse request", stderr);
        break;

    default:
        fprintf(stderr, "Unknown error: %d", err);
        break;
    }
    fputc('\n', stderr);
}

