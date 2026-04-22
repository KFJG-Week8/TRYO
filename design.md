# 설계 문서

## 아키텍처

런타임 흐름은 WEEK8 네트워크 프로그래밍 흐름을 그대로 따라갑니다.

```text
client
  -> TCP connect
  -> server accept()
  -> client fd enqueue
  -> worker thread dequeue
  -> HTTP parser
  -> SQL parser
  -> DB engine
  -> HTTP JSON response
  -> close client fd
```

main thread는 listening socket을 소유하고, worker thread는 thread-pool queue를 통해 전달받은 client socket을 처리합니다.

## 모듈

| 파일 | 책임 |
| --- | --- |
| `src/main.c` | 명령행 인자를 읽고 server 설정을 만듭니다. |
| `src/server.c` | BSD socket 설정, accept loop, routing, request logging을 담당합니다. |
| `src/thread_pool.c` | 고정 크기 worker pool과 bounded fd queue를 관리합니다. |
| `src/http.c` | 최소 HTTP request reader와 JSON response writer를 제공합니다. |
| `src/sql.c` | 지원하는 SQL 문법만 `SqlStatement`로 파싱합니다. |
| `src/db.c` | file-backed `users` table, read-write lock, query 실행을 담당합니다. |
| `src/bptree.c` | `id -> record index`를 저장하는 in-memory B+ tree입니다. |
| `src/util.c` | 시간 측정, JSON escaping, JSON builder 같은 공통 유틸을 제공합니다. |

## API 경계

네트워크 계층은 파일 형식이나 B+ tree 내부 구조를 알지 않습니다. 네트워크 계층은 SQL text를 받아 SQL parser에 넘기고, parser가 만든 구조체를 DB operation으로 바꾼 뒤, DB 결과를 JSON으로 직렬화합니다.

```text
HTTP layer
  -> SQL text
  -> SQL parser
  -> SqlStatement
  -> DB engine
  -> DbResult
  -> HTTP JSON response
```

## 동시성 모델

서버는 요청 단위로 동시성을 가집니다.

- `accept()`는 main thread에서 실행됩니다.
- accept된 client file descriptor는 thread-pool queue에 들어갑니다.
- worker thread는 queue에서 fd를 꺼내 독립적으로 요청을 처리합니다.

DB engine은 하나의 `pthread_rwlock_t`로 보호됩니다.

- `SELECT`는 `pthread_rwlock_rdlock`을 사용합니다.
- `INSERT`는 `pthread_rwlock_wrlock`을 사용합니다.

이 덕분에 여러 SELECT는 동시에 실행할 수 있고, INSERT는 파일 append와 메모리/index 갱신 중 배타적으로 실행됩니다.

## 저장과 시작

데이터 파일은 한 줄에 record 하나를 저장합니다.

```text
1,kim,20
2,lee,22
```

서버 시작 시 DB engine은 파일을 전부 읽어서 다음 상태를 복구합니다.

- memory record 배열
- 다음 auto-increment id
- `id -> record index` B+ tree

## B+ 트리 사용

B+ tree는 integer id와 record 배열 위치만 저장합니다. 사용되는 SQL은 다음 하나입니다.

```sql
SELECT * FROM users WHERE id = N;
```

다른 조건은 record 배열을 선형 탐색합니다. 이 차이를 응답의 `index_used`와 `elapsed_us`로 관찰할 수 있습니다.

## 실패 처리

지원하지 않는 SQL, 잘못된 JSON, 잘못된 HTTP 요청, 없는 route, DB 오류는 JSON error body를 반환합니다. 서버는 한 요청에 한 응답을 보낸 뒤 client connection을 닫으므로 HTTP keep-alive는 의도적으로 구현하지 않았습니다.
