#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>

typedef struct {
    int port;
    size_t thread_count;
    const char *data_path;
} ServerConfig;

int server_run(const ServerConfig *config);

#endif
