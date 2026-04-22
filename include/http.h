#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

#define HTTP_MAX_REQUEST 65536
#define HTTP_MAX_BODY 32768

typedef struct {
    char method[8];
    char path[128];
    char body[HTTP_MAX_BODY + 1];
    size_t body_len;
} HttpRequest;

int http_read_request(int client_fd, HttpRequest *request, char *err, size_t err_size);
int http_send_json(int client_fd, int status_code, const char *json_body);
int http_send_json_with_thread(int client_fd, int status_code, const char *json_body, unsigned long worker_thread_id);
int http_extract_sql(const char *body, char *sql_out, size_t sql_size, char *err, size_t err_size);

#endif
