#include "server.h"

#include "db.h"
#include "http.h"
#include "sql.h"
#include "thread_pool.h"
#include "util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define REQUEST_QUEUE_CAPACITY 128
#define SQL_MAX_LEN 4096

typedef struct {
    DbEngine db;
} ServerContext;

static volatile sig_atomic_t g_should_stop = 0;

static unsigned long current_worker_thread_id(void)
{
    return (unsigned long)pthread_self();
}

static void handle_signal(int signo)
{
    (void)signo;
    g_should_stop = 1;
}

static void install_signal_handlers(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);

    signal(SIGPIPE, SIG_IGN);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}

static char *make_error_body(const char *message)
{
    JsonBuilder builder;

    if (!json_builder_init(&builder)) {
        return NULL;
    }

    if (!json_builder_append(&builder, "{\"error\":") ||
        !json_builder_append_string(&builder, message) ||
        !json_builder_append(&builder, "}")) {
        json_builder_free(&builder);
        return NULL;
    }

    return json_builder_take(&builder);
}

static DbResult execute_statement(DbEngine *db, const SqlStatement *stmt)
{
    if (stmt->type == SQL_INSERT) {
        if (stmt->insert_has_id) {
            return db_insert_with_id(db, stmt->insert_id, stmt->insert_name, stmt->insert_age);
        }
        return db_insert(db, stmt->insert_name, stmt->insert_age);
    }

    DbFilter filter;
    DbProjection projection;
    memset(&filter, 0, sizeof(filter));
    memset(&projection, 0, sizeof(projection));

    if (stmt->where_type == SQL_WHERE_ID) {
        filter.type = DB_FILTER_ID;
        filter.id = stmt->where_id;
    } else if (stmt->where_type == SQL_WHERE_NAME) {
        filter.type = DB_FILTER_NAME;
        snprintf(filter.name, sizeof(filter.name), "%s", stmt->where_name);
    } else {
        filter.type = DB_FILTER_ALL;
    }

    if (stmt->select_all) {
        projection.include_id = true;
        projection.include_name = true;
        projection.include_age = true;
    } else {
        for (size_t i = 0; i < stmt->select_column_count; i++) {
            if (stmt->select_columns[i] == SQL_COLUMN_ID) {
                projection.include_id = true;
                projection.columns[projection.column_count++] = DB_COLUMN_ID;
            } else if (stmt->select_columns[i] == SQL_COLUMN_NAME) {
                projection.include_name = true;
                projection.columns[projection.column_count++] = DB_COLUMN_NAME;
            } else if (stmt->select_columns[i] == SQL_COLUMN_AGE) {
                projection.include_age = true;
                projection.columns[projection.column_count++] = DB_COLUMN_AGE;
            }
        }
    }

    return db_select_projected(db, filter, projection);
}

static void log_request(int client_fd, const HttpRequest *request, const char *detail, long long elapsed_us, int index_used)
{
    printf("request | thread=%lu | fd=%d | %s %s | result=%s | elapsed=%lldus | index_used=%s\n",
           current_worker_thread_id(),
           client_fd,
           request->method,
           request->path,
           detail,
           elapsed_us,
           index_used ? "true" : "false");
    fflush(stdout);
}

static int send_json_response(int client_fd, int status_code, const char *body)
{
    return http_send_json_with_thread(client_fd, status_code, body, current_worker_thread_id());
}

static void send_error_response(int client_fd, int status_code, const char *message)
{
    char *body = make_error_body(message);

    if (body == NULL) {
        send_json_response(client_fd, 500, "{\"error\":\"out of memory\"}");
        return;
    }

    send_json_response(client_fd, status_code, body);
    free(body);
}

static void handle_query(int client_fd, ServerContext *context, const HttpRequest *request)
{
    char sql[SQL_MAX_LEN];
    char err[256];
    SqlStatement stmt;
    DbResult db_result;
    const char *rows;

    if (!http_extract_sql(request->body, sql, sizeof(sql), err, sizeof(err))) {
        send_error_response(client_fd, 400, err);
        log_request(client_fd, request, "bad-body", 0, 0);
        return;
    }

    if (!sql_parse(sql, &stmt, err, sizeof(err))) {
        send_error_response(client_fd, 400, err);
        log_request(client_fd, request, "bad-sql", 0, 0);
        return;
    }

    db_result = execute_statement(&context->db, &stmt);
    if (!db_result.ok) {
        send_error_response(client_fd, 500, db_result.message);
        log_request(client_fd, request, "db-error", db_result.elapsed_us, db_result.index_used);
        db_result_free(&db_result);
        return;
    }

    rows = db_result.rows_json == NULL ? "[]" : db_result.rows_json;
    send_json_response(client_fd, 200, rows);
    log_request(client_fd, request, "ok", db_result.elapsed_us, db_result.index_used);
    db_result_free(&db_result);
}

static void handle_client(int client_fd, void *context)
{
    ServerContext *server_context = context;
    HttpRequest request;
    char err[256];

    if (!http_read_request(client_fd, &request, err, sizeof(err))) {
        send_error_response(client_fd, 400, err);
        close(client_fd);
        return;
    }

    if (strcasecmp(request.method, "GET") == 0 && strcmp(request.path, "/health") == 0) {
        send_json_response(client_fd, 200, "{\"status\":\"ok\"}");
        log_request(client_fd, &request, "ok", 0, 0);
    } else if (strcasecmp(request.method, "POST") == 0 && strcmp(request.path, "/query") == 0) {
        handle_query(client_fd, server_context, &request);
    } else if (strcmp(request.path, "/query") == 0 || strcmp(request.path, "/health") == 0) {
        send_error_response(client_fd, 405, "method not allowed");
        log_request(client_fd, &request, "method-not-allowed", 0, 0);
    } else {
        send_error_response(client_fd, 404, "route not found");
        log_request(client_fd, &request, "not-found", 0, 0);
    }

    close(client_fd);
}

static int create_listen_socket(int port)
{
    int listen_fd;
    int opt = 1;
    struct sockaddr_in addr;

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return -1;
    }

    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(listen_fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return -1;
    }

    if (listen(listen_fd, SOMAXCONN) < 0) {
        perror("listen");
        close(listen_fd);
        return -1;
    }

    return listen_fd;
}

int server_run(const ServerConfig *config)
{
    ServerContext context;
    ThreadPool pool;
    char err[256];
    int listen_fd;

    install_signal_handlers();

    if (!db_init(&context.db, config->data_path, err, sizeof(err))) {
        fprintf(stderr, "DB init failed: %s\n", err);
        return 1;
    }

    if (!thread_pool_init(&pool, config->thread_count, REQUEST_QUEUE_CAPACITY, handle_client, &context)) {
        fprintf(stderr, "Thread pool init failed\n");
        db_destroy(&context.db);
        return 1;
    }

    listen_fd = create_listen_socket(config->port);
    if (listen_fd < 0) {
        thread_pool_shutdown(&pool);
        db_destroy(&context.db);
        return 1;
    }

    printf("\n------------------------------------------------------------\n");
    printf("WEEK8 mini DBMS API server\n");
    printf("------------------------------------------------------------\n");
    printf("Listening   http://127.0.0.1:%d\n", config->port);
    printf("Workers     %zu\n", config->thread_count);
    printf("Data file   %s\n", config->data_path);
    printf("Stop        Ctrl+C\n");
    fflush(stdout);

    while (!g_should_stop) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (g_should_stop || errno == EBADF) {
                break;
            }
            perror("accept");
            continue;
        }

        if (!thread_pool_submit(&pool, client_fd)) {
            close(client_fd);
            break;
        }
    }

    if (listen_fd >= 0) {
        close(listen_fd);
    }

    thread_pool_shutdown(&pool);
    db_destroy(&context.db);
    printf("\nServer stopped\n");
    return 0;
}
