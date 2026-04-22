#include "server.h"

#include <stdio.h>
#include <stdlib.h>

static int parse_int_arg(const char *text, int fallback)
{
    char *end = NULL;
    long value = strtol(text, &end, 10);

    if (end == text || *end != '\0' || value <= 0 || value > 65535) {
        return fallback;
    }

    return (int)value;
}

int main(int argc, char **argv)
{
    ServerConfig config;

    config.port = 8080;
    config.thread_count = 4;
    config.data_path = "data/users.csv";

    if (argc > 1) {
        config.port = parse_int_arg(argv[1], config.port);
    }

    if (argc > 2) {
        int threads = parse_int_arg(argv[2], (int)config.thread_count);
        config.thread_count = (size_t)threads;
    }

    if (argc > 3) {
        config.data_path = argv[3];
    }

    return server_run(&config);
}
