// the default ulimit -Sn is usually 1024, but
// take the default file descriptors into account

#ifndef CONFIG
#define CONFIG

#define BACKLOG_SIZE    1000
#define BUFFER_SIZE     4096
#define MAX_BUFFERS     4096
#define MAX_HANDLERS    1000
#define DEFAULT_SERVICE "8080"

#endif

