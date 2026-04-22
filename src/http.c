#include "http.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

static void set_err(char *err, size_t err_size, const char *message)
{
    if (err_size > 0) {
        snprintf(err, err_size, "%s", message);
    }
}

static int send_all(int fd, const char *buffer, size_t len)
{
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, buffer + sent, len - sent, 0);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }

        if (n == 0) {
            return 0;
        }

        sent += (size_t)n;
    }

    return 1;
}

static int parse_content_length(const char *buffer, const char *headers_end, size_t *content_length)
{
    const char *line = strstr(buffer, "\r\n");

    *content_length = 0;
    if (line == NULL) {
        return 0;
    }
    line += 2;

    while (line < headers_end) {
        const char *next = strstr(line, "\r\n");
        size_t line_len;

        if (next == NULL || next > headers_end) {
            break;
        }

        line_len = (size_t)(next - line);
        if (line_len == 0) {
            break;
        }

        if (line_len >= 15 && strncasecmp(line, "Content-Length:", 15) == 0) {
            const char *value = line + 15;
            char *end = NULL;
            unsigned long parsed;

            while (*value != '\0' && isspace((unsigned char)*value)) {
                value++;
            }

            parsed = strtoul(value, &end, 10);
            if (end == value) {
                return 0;
            }

            *content_length = (size_t)parsed;
            return 1;
        }

        line = next + 2;
    }

    return 1;
}

int http_read_request(int client_fd, HttpRequest *request, char *err, size_t err_size)
{
    char buffer[HTTP_MAX_REQUEST + 1];
    size_t used = 0;
    char *headers_end = NULL;
    size_t header_len;
    size_t content_length;
    size_t total_needed;

    memset(request, 0, sizeof(*request));

    while (headers_end == NULL && used < HTTP_MAX_REQUEST) {
        ssize_t n = recv(client_fd, buffer + used, HTTP_MAX_REQUEST - used, 0);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_err(err, err_size, "failed to read request");
            return 0;
        }

        if (n == 0) {
            set_err(err, err_size, "client closed connection");
            return 0;
        }

        used += (size_t)n;
        buffer[used] = '\0';
        headers_end = strstr(buffer, "\r\n\r\n");
    }

    if (headers_end == NULL) {
        set_err(err, err_size, "request headers too large or incomplete");
        return 0;
    }

    if (sscanf(buffer, "%7s %127s", request->method, request->path) != 2) {
        set_err(err, err_size, "malformed request line");
        return 0;
    }

    if (!parse_content_length(buffer, headers_end, &content_length)) {
        set_err(err, err_size, "invalid Content-Length");
        return 0;
    }

    if (content_length > HTTP_MAX_BODY) {
        set_err(err, err_size, "request body too large");
        return 0;
    }

    header_len = (size_t)(headers_end - buffer) + 4;
    total_needed = header_len + content_length;
    if (total_needed > HTTP_MAX_REQUEST) {
        set_err(err, err_size, "request too large");
        return 0;
    }

    while (used < total_needed) {
        ssize_t n = recv(client_fd, buffer + used, total_needed - used, 0);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            set_err(err, err_size, "failed to read request body");
            return 0;
        }

        if (n == 0) {
            set_err(err, err_size, "incomplete request body");
            return 0;
        }

        used += (size_t)n;
    }

    memcpy(request->body, buffer + header_len, content_length);
    request->body[content_length] = '\0';
    request->body_len = content_length;
    return 1;
}

int http_send_json(int client_fd, int status_code, const char *json_body)
{
    const char *reason = "OK";
    char header[512];
    size_t body_len = strlen(json_body);
    int header_len;

    if (status_code == 400) {
        reason = "Bad Request";
    } else if (status_code == 404) {
        reason = "Not Found";
    } else if (status_code == 405) {
        reason = "Method Not Allowed";
    } else if (status_code >= 500) {
        reason = "Internal Server Error";
    }

    header_len = snprintf(header, sizeof(header),
                          "HTTP/1.1 %d %s\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: close\r\n"
                          "\r\n",
                          status_code, reason, body_len);

    if (header_len < 0 || (size_t)header_len >= sizeof(header)) {
        return 0;
    }

    return send_all(client_fd, header, (size_t)header_len) &&
           send_all(client_fd, json_body, body_len);
}

static int copy_raw_sql(const char *body, char *sql_out, size_t sql_size, char *err, size_t err_size)
{
    const char *start = body;
    const char *end;
    size_t len;

    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }

    end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }

    len = (size_t)(end - start);
    if (len == 0) {
        set_err(err, err_size, "SQL body is empty");
        return 0;
    }

    if (len + 1 > sql_size) {
        set_err(err, err_size, "SQL body is too long");
        return 0;
    }

    memcpy(sql_out, start, len);
    sql_out[len] = '\0';
    return 1;
}

static int extract_json_sql(const char *body, char *sql_out, size_t sql_size, char *err, size_t err_size)
{
    const char *p = strstr(body, "\"sql\"");
    size_t len = 0;

    if (p == NULL) {
        set_err(err, err_size, "JSON body must contain key \"sql\"");
        return 0;
    }

    p += 5;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }

    if (*p != ':') {
        set_err(err, err_size, "expected ':' after sql key");
        return 0;
    }
    p++;

    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }

    if (*p != '"') {
        set_err(err, err_size, "sql value must be a JSON string");
        return 0;
    }
    p++;

    while (*p != '\0' && *p != '"') {
        char ch = *p;

        if (ch == '\\') {
            p++;
            if (*p == '\0') {
                set_err(err, err_size, "unterminated JSON escape");
                return 0;
            }

            switch (*p) {
            case '"':
            case '\\':
            case '/':
                ch = *p;
                break;
            case 'n':
                ch = '\n';
                break;
            case 'r':
                ch = '\r';
                break;
            case 't':
                ch = '\t';
                break;
            default:
                set_err(err, err_size, "unsupported JSON escape in sql value");
                return 0;
            }
        }

        if (len + 1 >= sql_size) {
            set_err(err, err_size, "sql value is too long");
            return 0;
        }

        sql_out[len++] = ch;
        p++;
    }

    if (*p != '"') {
        set_err(err, err_size, "unterminated sql JSON string");
        return 0;
    }

    sql_out[len] = '\0';
    return 1;
}

int http_extract_sql(const char *body, char *sql_out, size_t sql_size, char *err, size_t err_size)
{
    const char *p = body;

    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }

    if (*p == '{') {
        return extract_json_sql(body, sql_out, sql_size, err, err_size);
    }

    return copy_raw_sql(body, sql_out, sql_size, err, err_size);
}
