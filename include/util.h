#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} JsonBuilder;

long long now_us(void);
char *json_escape_dup(const char *input);
int json_builder_init(JsonBuilder *builder);
void json_builder_free(JsonBuilder *builder);
int json_builder_append(JsonBuilder *builder, const char *text);
int json_builder_appendf(JsonBuilder *builder, const char *fmt, ...);
int json_builder_append_string(JsonBuilder *builder, const char *text);
char *json_builder_take(JsonBuilder *builder);

#endif
