#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

long long now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000LL + (long long)tv.tv_usec;
}

char *json_escape_dup(const char *input)
{
    size_t len = strlen(input);
    size_t cap = (len * 6) + 1;
    char *out = malloc(cap);
    size_t pos = 0;

    if (out == NULL) {
        return NULL;
    }

    for (size_t i = 0; input[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)input[i];

        switch (ch) {
        case '"':
            out[pos++] = '\\';
            out[pos++] = '"';
            break;
        case '\\':
            out[pos++] = '\\';
            out[pos++] = '\\';
            break;
        case '\n':
            out[pos++] = '\\';
            out[pos++] = 'n';
            break;
        case '\r':
            out[pos++] = '\\';
            out[pos++] = 'r';
            break;
        case '\t':
            out[pos++] = '\\';
            out[pos++] = 't';
            break;
        default:
            if (ch < 0x20) {
                int written = snprintf(out + pos, cap - pos, "\\u%04x", ch);
                if (written < 0) {
                    free(out);
                    return NULL;
                }
                pos += (size_t)written;
            } else {
                out[pos++] = (char)ch;
            }
            break;
        }
    }

    out[pos] = '\0';
    return out;
}
