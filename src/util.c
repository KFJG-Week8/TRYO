#include "util.h"

#include <stdint.h>
#include <stdarg.h>
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

int json_builder_init(JsonBuilder *builder)
{
    builder->cap = 256;
    builder->len = 0;
    builder->data = malloc(builder->cap);
    if (builder->data == NULL) {
        return 0;
    }
    builder->data[0] = '\0';
    return 1;
}

void json_builder_free(JsonBuilder *builder)
{
    free(builder->data);
    builder->data = NULL;
    builder->len = 0;
    builder->cap = 0;
}

static int json_builder_reserve(JsonBuilder *builder, size_t extra)
{
    size_t needed;
    size_t next_cap;

    if (extra > SIZE_MAX - builder->len - 1) {
        return 0;
    }

    needed = builder->len + extra + 1;

    if (needed <= builder->cap) {
        return 1;
    }

    next_cap = builder->cap == 0 ? 256 : builder->cap;
    while (next_cap < needed) {
        if (next_cap > SIZE_MAX / 2) {
            return 0;
        }
        next_cap *= 2;
    }

    char *next = realloc(builder->data, next_cap);
    if (next == NULL) {
        return 0;
    }

    builder->data = next;
    builder->cap = next_cap;
    return 1;
}

int json_builder_append(JsonBuilder *builder, const char *text)
{
    size_t len = strlen(text);

    if (!json_builder_reserve(builder, len)) {
        return 0;
    }

    memcpy(builder->data + builder->len, text, len + 1);
    builder->len += len;
    return 1;
}

int json_builder_appendf(JsonBuilder *builder, const char *fmt, ...)
{
    va_list ap;
    int written;

    while (1) {
        va_start(ap, fmt);
        written = vsnprintf(builder->data + builder->len, builder->cap - builder->len, fmt, ap);
        va_end(ap);

        if (written < 0) {
            return 0;
        }

        if (builder->len + (size_t)written < builder->cap) {
            builder->len += (size_t)written;
            return 1;
        }

        if (!json_builder_reserve(builder, (size_t)written + 1)) {
            return 0;
        }
    }
}

int json_builder_append_string(JsonBuilder *builder, const char *text)
{
    char *escaped = json_escape_dup(text);
    int ok;

    if (escaped == NULL) {
        return 0;
    }

    ok = json_builder_append(builder, "\"") &&
         json_builder_append(builder, escaped) &&
         json_builder_append(builder, "\"");
    free(escaped);
    return ok;
}

char *json_builder_take(JsonBuilder *builder)
{
    char *data = builder->data;

    builder->data = NULL;
    builder->len = 0;
    builder->cap = 0;
    return data;
}

char *json_escape_dup(const char *input)
{
    size_t len = strlen(input);
    size_t cap;
    char *out;
    size_t pos = 0;

    if (len > (SIZE_MAX - 1) / 6) {
        return NULL;
    }

    cap = (len * 6) + 1;
    out = malloc(cap);
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
