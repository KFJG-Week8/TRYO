# 8주차 구현 설명서: 함수 단위 실행 흐름과 코드

## 이 문서로 바로 얻는 것

이 문서는 8주차 요구사항인 `HTTP API 서버 + thread pool + SQL 처리기 연동 + 동시성 제어`가 이 프로젝트에서 어떻게 구현됐는지 함수 단위로 설명한다. 각 함수 설명 아래에 실제 코드 조각도 함께 넣어서, 문장을 읽고 바로 구현을 대조할 수 있게 구성했다.

핵심 흐름만 먼저 보면 이렇다.

1. [src/main.c](D:/Dprojects/TRYO/src/main.c:18)의 `main()`이 서버 설정을 만든다.
2. [src/server.c](D:/Dprojects/TRYO/src/server.c:272)의 `server_run()`이 DB, thread pool, 소켓을 초기화한다.
3. 메인 스레드는 `accept()`만 담당하고, worker thread가 실제 요청을 처리한다.
4. worker는 HTTP 요청에서 SQL을 추출하고 파싱한 뒤 DB 엔진을 실행한다.
5. 결과는 `rows`, `message`, `index_used`, `elapsed_us`를 담은 JSON으로 응답된다.

## 8주차 요구사항 기준으로 구현된 것

현재 코드에서 8주차 요구사항에 직접 대응되는 구현은 다음과 같다.

- HTTP API 서버 제공
- `GET /health`, `POST /query` 라우트 제공
- thread pool 기반 병렬 요청 처리
- SQL parser와 DB 엔진을 API 서버에 연결
- `pthread_rwlock_t` 기반 동시성 제어

즉, 이 프로젝트의 8주차 포인트는 새로운 SQL 문법을 더 만드는 것이 아니라, 이미 만든 SQL/DB 코어를 네트워크 API로 연결하고 여러 요청을 동시에 안전하게 처리하는 데 있다.

## 전체 실행 흐름

전체 흐름은 아래 순서로 움직인다.

1. [src/main.c](D:/Dprojects/TRYO/src/main.c:18)의 `main()`이 포트, worker 수, 데이터 파일 경로를 읽는다.
2. `main()`이 [src/server.c](D:/Dprojects/TRYO/src/server.c:272)의 `server_run()`을 호출한다.
3. `server_run()`이 DB 초기화, thread pool 생성, listening socket 생성을 수행한다.
4. 메인 스레드는 `accept()`로 연결을 받는다.
5. 연결은 [src/thread_pool.c](D:/Dprojects/TRYO/src/thread_pool.c:100)의 `thread_pool_submit()`으로 큐에 들어간다.
6. worker thread는 [src/thread_pool.c](D:/Dprojects/TRYO/src/thread_pool.c:6)의 `worker_loop()`에서 `client_fd`를 꺼낸다.
7. worker는 [src/server.c](D:/Dprojects/TRYO/src/server.c:206)의 `handle_client()`를 실행한다.
8. `/query` 요청이면 [src/server.c](D:/Dprojects/TRYO/src/server.c:164)의 `handle_query()`가 SQL 처리 흐름으로 넘긴다.
9. `handle_query()`는 `http_extract_sql() -> sql_parse() -> execute_statement() -> DB 실행 -> JSON 응답` 순서로 진행한다.

## 시작점: `main()`과 서버 설정

### `parse_int_arg()`

위치: [src/main.c](D:/Dprojects/TRYO/src/main.c:6)

이 함수는 커맨드라인 인자를 정수로 바꾸는 보조 함수다. 숫자가 아니거나 범위를 벗어나면 fallback 값을 유지한다.

```c
static int parse_int_arg(const char *text, int fallback)
{
    char *end = NULL;
    long value = strtol(text, &end, 10);

    if (end == text || *end != '\0' || value <= 0 || value > 65535) {
        return fallback;
    }

    return (int)value;
}
```

이 함수 덕분에 포트와 thread 수를 비교적 안전하게 읽는다.

### `main()`

위치: [src/main.c](D:/Dprojects/TRYO/src/main.c:18)

`main()`은 서버 시작에 필요한 기본 설정을 만들고, 인자가 있으면 이를 덮어쓴 뒤 `server_run()`을 호출한다.

```c
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
```

즉, 8주차 로직의 진짜 본체는 `server_run()`부터 시작된다.

## 서버 생명주기: `server_run()` 중심 설명

### `install_signal_handlers()`

위치: [src/server.c](D:/Dprojects/TRYO/src/server.c:37)

이 함수는 서버 종료를 깔끔하게 처리하기 위한 준비 단계다. `SIGPIPE`는 무시하고, `SIGINT`, `SIGTERM`이 오면 종료 플래그를 세운다.

```c
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
```

덕분에 서버는 강제 종료 대신 메인 루프를 빠져나와 자원을 정리하는 흐름으로 종료된다.

### `create_listen_socket()`

위치: [src/server.c](D:/Dprojects/TRYO/src/server.c:234)

이 함수는 TCP 소켓 생성, 옵션 설정, 바인드, listen까지 서버 수신 준비를 담당한다.

```c
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
```

이 함수가 성공해야 서버가 외부 요청을 받을 수 있다.

### `server_run()`

위치: [src/server.c](D:/Dprojects/TRYO/src/server.c:272)

이 함수가 8주차 서버 전체를 묶는 중심 함수다. DB 초기화, thread pool 준비, 소켓 생성, `accept()` 루프, 종료 시 정리까지 전부 들어 있다.

```c
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
    printf("Server stopped\n");
    return 0;
}
```

이 함수의 핵심 설계는 메인 스레드가 연결 수락만 하고, 실제 비즈니스 로직은 worker에게 넘긴다는 점이다.

## 요청 분산: thread pool이 어떻게 동작하는가

### `thread_pool_init()`

위치: [src/thread_pool.c](D:/Dprojects/TRYO/src/thread_pool.c:35)

이 함수는 요청을 처리할 worker thread들을 미리 만든다.

```c
int thread_pool_init(ThreadPool *pool, size_t thread_count, size_t queue_capacity, TaskHandler handler, void *context)
{
    ...
    pool->fds = malloc(sizeof(int) * queue_capacity);
    pool->threads = malloc(sizeof(pthread_t) * thread_count);
    ...
    pool->capacity = queue_capacity;
    pool->handler = handler;
    pool->context = context;
    ...
    for (size_t i = 0; i < thread_count; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_loop, pool) != 0) {
            thread_pool_shutdown(pool);
            return 0;
        }
        pool->thread_count++;
    }

    return 1;
}
```

초기화가 끝나면 worker들은 `worker_loop()` 안에서 요청이 들어오기를 기다린다.

### `thread_pool_submit()`

위치: [src/thread_pool.c](D:/Dprojects/TRYO/src/thread_pool.c:100)

메인 스레드가 `accept()`한 소켓을 큐에 넣는 함수다.

```c
int thread_pool_submit(ThreadPool *pool, int client_fd)
{
    pthread_mutex_lock(&pool->mutex);

    while (!pool->stopping && pool->count == pool->capacity) {
        pthread_cond_wait(&pool->not_full, &pool->mutex);
    }

    if (pool->stopping) {
        pthread_mutex_unlock(&pool->mutex);
        return 0;
    }

    pool->fds[pool->tail] = client_fd;
    pool->tail = (pool->tail + 1) % pool->capacity;
    pool->count++;
    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->mutex);
    return 1;
}
```

즉, 요청이 몰려도 큐에 잠시 쌓아두고 worker가 꺼내 처리한다.

### `worker_loop()`

위치: [src/thread_pool.c](D:/Dprojects/TRYO/src/thread_pool.c:6)

각 worker thread가 실제로 도는 무한 루프다.

```c
static void *worker_loop(void *arg)
{
    ThreadPool *pool = arg;

    while (1) {
        int client_fd;

        pthread_mutex_lock(&pool->mutex);
        while (!pool->stopping && pool->count == 0) {
            pthread_cond_wait(&pool->not_empty, &pool->mutex);
        }

        if (pool->stopping && pool->count == 0) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        client_fd = pool->fds[pool->head];
        pool->head = (pool->head + 1) % pool->capacity;
        pool->count--;
        pthread_cond_signal(&pool->not_full);
        pthread_mutex_unlock(&pool->mutex);

        pool->handler(client_fd, pool->context);
    }

    return NULL;
}
```

이 프로젝트에서는 `pool->handler`로 `handle_client()`가 들어간다. 즉, worker 하나가 클라이언트 연결 하나를 끝까지 처리한다.

## HTTP 계층: 요청을 읽고 SQL을 꺼내는 함수들

### `http_read_request()`

위치: [src/http.c](D:/Dprojects/TRYO/src/http.c:90)

이 함수는 소켓에서 HTTP 요청을 읽어 `HttpRequest` 구조체로 변환한다.

```c
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
        ...
        buffer[used] = '\0';
        headers_end = strstr(buffer, "\r\n\r\n");
    }

    ...

    memcpy(request->body, buffer + header_len, content_length);
    request->body[content_length] = '\0';
    request->body_len = content_length;
    return 1;
}
```

헤더를 먼저 모으고, `Content-Length`를 계산한 뒤, 필요한 길이만큼 body를 읽는 방식이다.

### `parse_content_length()`

위치: [src/http.c](D:/Dprojects/TRYO/src/http.c:43)

이 함수는 헤더 영역을 순회하면서 `Content-Length` 값을 찾아낸다.

```c
static int parse_content_length(const char *buffer, const char *headers_end, size_t *content_length)
{
    ...
    if (line_len >= 15 && strncasecmp(line, "Content-Length:", 15) == 0) {
        const char *value = line + 15;
        char *end = NULL;
        unsigned long parsed;
        ...
        parsed = strtoul(value, &end, 10);
        ...
        *content_length = (size_t)parsed;
        return 1;
    }
    ...
}
```

### `http_extract_sql()`

위치: [src/http.c](D:/Dprojects/TRYO/src/http.c:318)

이 함수는 `/query` body에서 SQL 문장만 뽑아낸다. body가 `{`로 시작하면 JSON으로 간주하고, 아니면 raw SQL로 처리한다.

```c
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
```

즉, 이 서버는 아래 두 입력을 모두 받는다.

```text
SELECT * FROM users WHERE id = 1;
```

```json
{"sql":"SELECT * FROM users WHERE id = 1;"}
```

### `copy_raw_sql()`

위치: [src/http.c](D:/Dprojects/TRYO/src/http.c:207)

raw body 앞뒤 공백을 걷어내고 SQL 문자열만 복사한다.

```c
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
    ...
    memcpy(sql_out, start, len);
    sql_out[len] = '\0';
    return 1;
}
```

### `extract_json_sql()`

위치: [src/http.c](D:/Dprojects/TRYO/src/http.c:238)

아주 작은 JSON parser 역할을 한다. `"sql"` 키를 찾고, 값이 문자열인지 확인한 뒤 SQL 내용을 추출한다.

```c
static int extract_json_sql(const char *body, char *sql_out, size_t sql_size, char *err, size_t err_size)
{
    const char *p = strstr(body, "\"sql\"");
    size_t len = 0;

    if (p == NULL) {
        set_err(err, err_size, "JSON body must contain key \"sql\"");
        return 0;
    }

    ...

    while (*p != '\0' && *p != '"') {
        char ch = *p;

        if (ch == '\\') {
            ...
        }

        sql_out[len++] = ch;
        p++;
    }

    sql_out[len] = '\0';
    return 1;
}
```

### `http_send_json()`

위치: [src/http.c](D:/Dprojects/TRYO/src/http.c:174)

JSON 응답용 HTTP 헤더를 만들고 body까지 전송한다.

```c
int http_send_json(int client_fd, int status_code, const char *json_body)
{
    const char *reason = "OK";
    char header[512];
    size_t body_len = strlen(json_body);
    int header_len;

    ...

    header_len = snprintf(header, sizeof(header),
                          "HTTP/1.1 %d %s\r\n"
                          "Content-Type: application/json\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: close\r\n"
                          "\r\n",
                          status_code, reason, body_len);

    return send_all(client_fd, header, (size_t)header_len) &&
           send_all(client_fd, json_body, body_len);
}
```

항상 `Content-Type`, `Content-Length`, `Connection: close`가 포함된다.

## 라우팅과 비즈니스 흐름: `handle_client()`와 `handle_query()`

### `handle_client()`

위치: [src/server.c](D:/Dprojects/TRYO/src/server.c:206)

worker thread가 실제로 클라이언트 연결 하나를 처리하는 진입점이다.

```c
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
        http_send_json(client_fd, 200, "{\"status\":\"ok\"}");
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
```

즉, `handle_client()`는 네트워크 라우터이고, 실제 SQL 실행은 `/query` 분기에서만 일어난다.

### `handle_query()`

위치: [src/server.c](D:/Dprojects/TRYO/src/server.c:164)

이 함수가 HTTP 요청을 SQL 실행 흐름으로 연결하는 핵심 어댑터다.

```c
static void handle_query(int client_fd, ServerContext *context, const HttpRequest *request)
{
    char sql[SQL_MAX_LEN];
    char err[256];
    SqlStatement stmt;
    DbResult db_result;
    char *body;

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

    body = make_success_body(&db_result);
    ...
    http_send_json(client_fd, 200, body);
    ...
    db_result_free(&db_result);
}
```

중요한 점은 에러가 어디서 났는지 단계별로 분리된다는 것이다.

- body 추출 실패: 400
- SQL 파싱 실패: 400
- DB 실행 실패: 500
- 응답 직렬화 실패: 500

## SQL 결과를 DB 호출로 연결하는 함수

### `execute_statement()`

위치: [src/server.c](D:/Dprojects/TRYO/src/server.c:92)

이 함수는 `SqlStatement`를 실제 DB 계층이 이해하는 형태로 번역한다.

```c
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

    ...

    return db_select_projected(db, filter, projection);
}
```

서버는 SQL 내부 구조를 자세히 몰라도 되고, 이 함수가 SQL parser와 DB 계층 사이의 중간 어댑터 역할을 한다.

## DB 계층: 파일 + 메모리 + 인덱스를 함께 쓰는 방식

### `db_init()`

위치: [src/db.c](D:/Dprojects/TRYO/src/db.c:165)

서버 시작 시 DB 엔진을 준비하는 함수다. 파일을 열고, 레코드를 메모리에 올리고, B+ tree 인덱스도 함께 구성한다.

```c
int db_init(DbEngine *db, const char *data_path, char *err, size_t err_size)
{
    FILE *file;
    char line[512];

    memset(db, 0, sizeof(*db));
    db->next_id = 1;

    ...

    if (pthread_rwlock_init(&db->lock, NULL) != 0) {
        set_err(err, err_size, "failed to initialize DB lock");
        return 0;
    }

    bptree_init(&db->index);

    file = fopen(db->data_path, "a+");
    ...

    rewind(file);
    while (fgets(line, sizeof(line), file) != NULL) {
        Record record;
        char name[DB_NAME_MAX];
        ...
        if (!load_record(db, &record)) {
            ...
            return 0;
        }
    }

    fclose(file);
    return 1;
}
```

즉, 이 서버는 시작 시 파일을 메모리로 적재하고 이후 SELECT는 메모리 기반으로 처리한다.

### `load_record()`

위치: [src/db.c](D:/Dprojects/TRYO/src/db.c:147)

파일에서 읽은 레코드 한 줄을 메모리 배열과 B+ tree 인덱스에 동시에 반영한다.

```c
static int load_record(DbEngine *db, const Record *record)
{
    if (!db_reserve_records(db, db->count + 1)) {
        return 0;
    }

    db->records[db->count] = *record;
    if (!bptree_insert(&db->index, record->id, db->count)) {
        return 0;
    }

    db->count++;
    if (record->id >= db->next_id) {
        db->next_id = record->id + 1;
    }
    return 1;
}
```

이 함수 덕분에 재시작 이후에도 파일 데이터가 인덱스와 함께 복구된다.

### `db_insert_internal()`

위치: [src/db.c](D:/Dprojects/TRYO/src/db.c:239)

실제 INSERT의 본체다. `db_insert()`와 `db_insert_with_id()`는 모두 이 함수로 들어온다.

```c
static DbResult db_insert_internal(DbEngine *db, int has_id, int id, const char *name, int age)
{
    long long start_us = now_us();
    ...

    if (pthread_rwlock_wrlock(&db->lock) != 0) {
        return make_error_result("failed to acquire write lock", start_us);
    }

    if ((has_id && id <= 0) || age < 0 || !name_is_valid(name)) {
        pthread_rwlock_unlock(&db->lock);
        return make_error_result("invalid id, name, or age", start_us);
    }

    ...

    file = fopen(db->data_path, "a");
    ...
    fprintf(file, "%d,%s,%d\n", record.id, record.name, record.age);
    ...

    db->records[record_index] = record;
    db->count++;
    ...
    bptree_insert(&db->index, record.id, record_index);
    ...

    result.ok = true;
    result.rows_json = json_builder_take(&builder);
    snprintf(result.message, sizeof(result.message), "inserted 1 row");
    result.index_used = false;
    result.elapsed_us = now_us() - start_us;

    pthread_rwlock_unlock(&db->lock);
    return result;
}
```

즉, INSERT는 한 번의 write lock 구간 안에서 아래 세 작업을 모두 끝낸다.

- 데이터 파일 append
- 메모리 배열 반영
- B+ tree 인덱스 반영

### `db_select_projected()`

위치: [src/db.c](D:/Dprojects/TRYO/src/db.c:329)

SELECT의 핵심 함수다. projection 처리, read lock, 인덱스 조회, 선형 탐색, 결과 JSON 생성이 모두 들어 있다.

```c
DbResult db_select_projected(DbEngine *db, DbFilter filter, DbProjection projection)
{
    long long start_us = now_us();
    ...

    if (pthread_rwlock_rdlock(&db->lock) != 0) {
        return make_error_result("failed to acquire read lock", start_us);
    }

    ...

    if (filter.type == DB_FILTER_ID) {
        size_t record_index = 0;

        if (bptree_search(&db->index, filter.id, &record_index) && record_index < db->count) {
            if (!append_record_json(&builder, &db->records[record_index], projection)) {
                ...
            }
            matched++;
        }
    } else {
        for (size_t i = 0; i < db->count; i++) {
            int include = filter.type == DB_FILTER_ALL ||
                          (filter.type == DB_FILTER_NAME && strcmp(db->records[i].name, filter.name) == 0);

            if (!include) {
                continue;
            }

            ...
            matched++;
        }
    }

    ...
    result.ok = true;
    result.rows_json = json_builder_take(&builder);
    snprintf(result.message, sizeof(result.message), "selected %zu row(s)", matched);
    result.index_used = filter.type == DB_FILTER_ID;
    result.elapsed_us = now_us() - start_us;

    pthread_rwlock_unlock(&db->lock);
    return result;
}
```

여기서 핵심 분기는 다음과 같다.

- `DB_FILTER_ID`: B+ tree 사용
- 그 외: 메모리 배열 선형 순회

그래서 `WHERE id = ?`는 인덱스를 타고, `WHERE name = ?`는 선형 탐색으로 처리된다.

### `db_result_free()`

위치: [src/db.c](D:/Dprojects/TRYO/src/db.c:410)

DB 결과 구조체 안의 `rows_json` 메모리를 해제한다.

```c
void db_result_free(DbResult *result)
{
    free(result->rows_json);
    result->rows_json = NULL;
}
```

`handle_query()`가 요청 처리 뒤 반드시 호출해 메모리 누수를 막는다.

## 응답 생성 함수: 사용자에게 실제로 무엇이 돌아가는가

### `make_success_body()`

위치: [src/server.c](D:/Dprojects/TRYO/src/server.c:68)

DB 결과를 최종 API 응답 JSON으로 감싼다.

```c
static char *make_success_body(const DbResult *result)
{
    JsonBuilder builder;
    const char *rows = result->rows_json == NULL ? "[]" : result->rows_json;
    const char *index_used = result->index_used ? "true" : "false";

    if (!json_builder_init(&builder)) {
        return NULL;
    }

    if (!json_builder_append(&builder, "{\"ok\":true,\"rows\":") ||
        !json_builder_append(&builder, rows) ||
        !json_builder_append(&builder, ",\"message\":") ||
        !json_builder_append_string(&builder, result->message) ||
        !json_builder_append(&builder, ",\"index_used\":") ||
        !json_builder_append(&builder, index_used) ||
        !json_builder_appendf(&builder, ",\"elapsed_us\":%lld}", result->elapsed_us)) {
        json_builder_free(&builder);
        return NULL;
    }

    return json_builder_take(&builder);
}
```

즉, 클라이언트는 데이터뿐 아니라 인덱스 사용 여부와 수행 시간까지 함께 받는다.

### `make_error_body()`

위치: [src/server.c](D:/Dprojects/TRYO/src/server.c:50)

에러 메시지를 일관된 JSON 형식으로 만든다.

```c
static char *make_error_body(const char *message)
{
    JsonBuilder builder;

    if (!json_builder_init(&builder)) {
        return NULL;
    }

    if (!json_builder_append(&builder, "{\"ok\":false,\"error\":") ||
        !json_builder_append_string(&builder, message) ||
        !json_builder_append(&builder, "}")) {
        json_builder_free(&builder);
        return NULL;
    }

    return json_builder_take(&builder);
}
```

### `send_error_response()`

위치: [src/server.c](D:/Dprojects/TRYO/src/server.c:151)

상태 코드와 메시지를 받아 JSON 에러 응답으로 보내는 공통 함수다.

```c
static void send_error_response(int client_fd, int status_code, const char *message)
{
    char *body = make_error_body(message);

    if (body == NULL) {
        http_send_json(client_fd, 500, "{\"ok\":false,\"error\":\"out of memory\"}");
        return;
    }

    http_send_json(client_fd, status_code, body);
    free(body);
}
```

### `log_request()`

위치: [src/server.c](D:/Dprojects/TRYO/src/server.c:138)

요청 처리 결과를 로그로 남긴다.

```c
static void log_request(int client_fd, const HttpRequest *request, const char *detail, long long elapsed_us, int index_used)
{
    printf("[thread=%lu fd=%d] %s %s %s elapsed_us=%lld index_used=%s\n",
           (unsigned long)pthread_self(),
           client_fd,
           request->method,
           request->path,
           detail,
           elapsed_us,
           index_used ? "true" : "false");
    fflush(stdout);
}
```

이 로그 덕분에 응답 결과뿐 아니라 어떤 요청이 얼마나 걸렸는지도 운영 관점에서 확인할 수 있다.

## 실제 요청 예시와 결과

### 1. 상태 확인

요청:

```http
GET /health HTTP/1.1
Host: localhost:8080
```

결과:

```json
{"status":"ok"}
```

### 2. raw SQL 조회 요청

요청:

```http
POST /query HTTP/1.1
Host: localhost:8080
Content-Length: 31

SELECT * FROM users WHERE id = 1;
```

처리 흐름:

`http_read_request()` -> `http_extract_sql()` -> `sql_parse()` -> `execute_statement()` -> `db_select_projected()` -> `make_success_body()` -> `http_send_json()`

가능한 결과 예시:

```json
{
  "ok": true,
  "rows": [{"id":1,"name":"kim","age":20}],
  "message": "selected 1 row(s)",
  "index_used": true,
  "elapsed_us": 41
}
```

### 3. JSON body INSERT 요청

요청:

```http
POST /query HTTP/1.1
Host: localhost:8080
Content-Length: 47

{"sql":"INSERT INTO users name age VALUES 'lee' 22;"}
```

가능한 결과 예시:

```json
{
  "ok": true,
  "rows": [{"id":2,"name":"lee","age":22}],
  "message": "inserted 1 row",
  "index_used": false,
  "elapsed_us": 87
}
```

### 4. 잘못된 SQL 요청

가능한 결과 예시:

```json
{
  "ok": false,
  "error": "only WHERE id or WHERE name is supported"
}
```

## 함수 기준으로 다시 보는 8주차 핵심 포인트

- [src/server.c](D:/Dprojects/TRYO/src/server.c:272)의 `server_run()`이 서버 생명주기를 총괄한다.
- [src/thread_pool.c](D:/Dprojects/TRYO/src/thread_pool.c:35)의 thread pool이 병렬 요청 처리를 맡는다.
- [src/server.c](D:/Dprojects/TRYO/src/server.c:164)의 `handle_query()`가 HTTP와 SQL/DB를 연결한다.
- [src/db.c](D:/Dprojects/TRYO/src/db.c:329)의 `db_select_projected()`와 [src/db.c](D:/Dprojects/TRYO/src/db.c:239)의 `db_insert_internal()`이 실제 데이터를 읽고 쓴다.

## 마무리

이 프로젝트의 8주차 구현은 "웹 서버를 만들었다"보다 "이미 만든 SQL parser, DB 엔진, B+ tree 인덱스를 HTTP API 서버에 안정적으로 연결했다"는 데 의미가 있다. 메인 스레드는 연결만 받고, worker thread가 요청을 분산 처리하며, DB는 read-write lock으로 보호되고, 결과는 일관된 JSON 형식으로 반환된다.

발표나 설명을 시작할 때는 아래 문장으로 열면 흐름이 잘 잡힌다.

> 이 서버는 `main()`이 설정을 만들고, `server_run()`이 DB와 thread pool과 소켓을 초기화한 뒤, worker thread가 `handle_client()`와 `handle_query()`를 통해 SQL 요청을 DB 실행으로 연결하고, 최종 결과를 JSON으로 응답하는 구조입니다.
