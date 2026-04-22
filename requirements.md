# WEEK8 미니 DBMS API 서버 요구사항

## 목표

BSD socket을 사용하는 C 언어 미니 DBMS API 서버를 만든다. 외부 클라이언트는 HTTP로 SQL을 보내고, 서버는 파일 기반 DB engine과 in-memory B+ tree index를 사용해 SQL을 실행한다.

이 프로젝트는 하루짜리 bootcamp 학습 범위 안에 들어올 정도로 작게 유지하되, WEEK8의 핵심 개념인 TCP socket, file descriptor, HTTP, thread pool, DB concurrency를 한 흐름 안에서 볼 수 있어야 한다.

## 핵심 학습 키워드

- BSD socket
- IP / TCP
- HTTP request와 response
- file descriptor
- DNS / localhost
- thread pool
- concurrency control
- file-based DB
- SQL parser
- B+ tree index

## 지원 API

### GET /health

서버 상태 확인 API입니다.

응답:

```json
{"status":"ok"}
```

### POST /query

SQL 실행 API입니다.

요청:

```json
{"sql":"SELECT * FROM users WHERE id = 1;"}
```

성공 응답:

```json
{"ok":true,"rows":[],"message":"success","index_used":false,"elapsed_us":10}
```

실패 응답:

```json
{"ok":false,"error":"reason"}
```

## 지원 SQL

- `INSERT INTO users name age VALUES 'kim' 20;`
- `SELECT * FROM users;`
- `SELECT * FROM users WHERE id = 1;`
- `SELECT * FROM users WHERE name = 'kim';`

## 테이블 가정

테이블은 `users` 하나만 존재합니다.

| column | 의미 |
| --- | --- |
| `id` | auto-increment integer |
| `name` | string |
| `age` | integer |

## 저장 방식

- 데이터는 `id,name,age` 형식의 CSV-like file에 저장합니다.
- 서버 시작 시 DB engine은 파일을 읽어 memory record 배열과 B+ tree index를 다시 만듭니다.
- `INSERT`는 data file 끝에 한 줄을 append합니다.

## 인덱스

- `id` column은 in-memory B+ tree로 인덱싱합니다.
- `WHERE id = ?`는 B+ tree를 사용합니다.
- 다른 검색은 선형 탐색을 사용합니다.

## 동시성

- 서버는 고정 크기 thread pool을 사용합니다.
- accept loop는 client file descriptor를 bounded queue에 넣습니다.
- worker thread는 client fd를 꺼내 HTTP parsing, SQL 실행, response write, fd close를 수행합니다.
- DB engine은 하나의 global read-write lock을 사용합니다.
- `SELECT`는 read lock을 잡습니다.
- `INSERT`는 write lock을 잡습니다.

## 범위 밖

- `CREATE TABLE`
- `UPDATE`
- `DELETE`
- `JOIN`
- 복잡한 `WHERE` 조건
- 전체 SQL grammar
- 전체 JSON parser
- HTTPS
- HTTP keep-alive
- production-grade HTTP 동작

## 성공 기준

- `make`로 server binary가 빌드된다.
- `make test`가 parser, B+ tree, DB 테스트를 통과한다.
- 서버가 curl 요청으로 health, insert, select-all, select-by-id, select-by-name을 처리한다.
- concurrent request가 파일이나 index를 망가뜨리지 않는다.
- log 또는 benchmark 결과로 indexed lookup과 linear scan의 차이를 설명할 수 있다.
