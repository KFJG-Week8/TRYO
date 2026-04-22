# 02. 작동 원리: 요청 하나가 시스템을 통과하는 방식

## 먼저 한 문장으로 보기
이 문서는 이 프로젝트를 처음으로 깊게 설명하는 본문입니다. `curl` 요청 하나가 서버에 들어와 SQL 실행 결과로 돌아가기까지, 각 단계가 왜 필요하고 어떤 CS 개념을 담고 있는지 따라갑니다.

## 전체 요청 지도
```text
main()
  -> server_run()
  -> create_listen_socket()
  -> accept()
  -> thread_pool_submit(client_fd)
  -> worker_loop()
  -> handle_client(client_fd)
  -> http_read_request()
  -> handle_query()
  -> http_extract_sql()
  -> sql_parse()
  -> execute_statement()
  -> db_insert() or db_select()
  -> bptree_search() if WHERE id
  -> http_send_json()
  -> close(client_fd)
```

이 지도는 함수 호출 목록이기도 하지만, 더 중요하게는 **책임이 이동하는 경로**입니다.

| 구간 | 책임 | 핵심 개념 |
| --- | --- | --- |
| 서버 시작 | 프로그램을 네트워크 서버로 준비 | process, socket, port |
| 연결 수락 | 클라이언트별 통신 통로 생성 | TCP, `accept`, file descriptor |
| 작업 분배 | fd를 worker thread에게 전달 | thread pool, queue, mutex |
| 요청 해석 | byte stream을 HTTP request로 변환 | HTTP framing, `Content-Length` |
| SQL 구조화 | 문자열을 실행 의도로 변환 | parsing, structured data |
| DB 실행 | 저장소와 인덱스로 데이터 처리 | persistence, memory state, B+ tree |
| 응답 전송 | 결과를 HTTP JSON으로 반환 | serialization, observability |

## 파일별 책임: 단순 목록이 아니라 계약으로 보기
| 파일 | 입력 | 출력 | 책임 | 관련 CS 개념 |
| --- | --- | --- | --- | --- |
| `src/main.c` | 실행 인자 | `ServerConfig` | 서버 설정을 만들고 시작점을 호출 | process entry point |
| `src/server.c` | port, thread 수, data path, client fd | HTTP response | socket lifecycle, routing, request orchestration | BSD socket, TCP, routing |
| `src/thread_pool.c` | client fd | worker handler 호출 | fd queue와 worker thread 관리 | thread, mutex, condition variable |
| `src/http.c` | socket fd의 byte stream | `HttpRequest`, HTTP response | HTTP request/response 최소 파싱 | byte stream, message framing |
| `src/sql.c` | SQL 문자열 | `SqlStatement` | 지원 SQL 문법을 구조화 | parsing, validation |
| `src/db.c` | `SqlStatement`에서 변환된 DB operation | `DbResult` | 파일, 메모리, lock, 인덱스 조합 | persistence, concurrency |
| `src/bptree.c` | integer key와 record index | 빠른 key lookup | id index 관리 | B+ tree, search complexity |
| `tests/test_main.c` | 테스트 입력 | assert 결과 | 주요 동작 검증 | unit test, regression |

“계약”이라는 관점이 중요합니다. `http.c`는 SQL 문법을 몰라도 됩니다. `sql.c`는 socket fd를 몰라도 됩니다. `bptree.c`는 HTTP response를 몰라도 됩니다. 이렇게 관심사를 나누면 한 부분을 바꿔도 다른 부분이 덜 흔들립니다.

## 1단계: 프로그램은 어떻게 서버가 되는가
`src/main.c`는 프로그램의 입구입니다. 사용자가 다음처럼 실행할 수 있습니다.

```sh
./bin/week8_dbms 8080 4 data/users.csv
```

이 실행 인자는 서버가 어떤 port에서 기다릴지, worker thread를 몇 개 만들지, 어떤 파일을 DB 저장소로 쓸지 정합니다. `main()`은 이 값을 `ServerConfig`에 담고 `server_run()`을 호출합니다.

여기서 첫 번째 Top-Down 관점이 나옵니다. 서버는 “그냥 실행되는 프로그램”이 아니라, 운영체제에게 네트워크 자원을 요청한 프로그램입니다.

`src/server.c`의 `create_listen_socket()`은 다음 단계를 거칩니다.

```text
socket(AF_INET, SOCK_STREAM, 0)
  -> setsockopt(SO_REUSEADDR)
  -> bind(INADDR_ANY, port)
  -> listen(SOMAXCONN)
```

각 단계는 서버의 상태를 조금씩 바꿉니다.

| 호출 | 의미 |
| --- | --- |
| `socket` | TCP 통신용 fd를 만든다. |
| `setsockopt` | socket 동작 옵션을 설정한다. |
| `bind` | 이 fd를 특정 port에 묶는다. |
| `listen` | 새 연결을 받을 수 있는 listening socket으로 만든다. |

입문자는 `socket()`을 “네트워크 연결”이라고 바로 생각하기 쉽지만, 정확히는 아직 연결이 아닙니다. `socket()`은 통신에 사용할 수 있는 자원을 만든 것입니다. 서버가 어떤 주소와 port에서 기다릴지는 `bind()`가 정하고, 연결 대기 상태는 `listen()`이 만듭니다.

### 최소구현의 선택
이 서버는 IPv4 TCP socket만 사용합니다. TLS, IPv6, non-blocking socket, event loop는 다루지 않습니다. 덕분에 socket lifecycle의 기본 순서가 선명하게 보입니다.

## 2단계: `accept()`는 왜 특별한가
서버가 `listen()` 상태가 되면 클라이언트 연결을 받을 수 있습니다. 하지만 listening socket fd 자체로 클라이언트와 대화하는 것은 아닙니다. `accept()`가 새 연결 하나를 받아 **client fd**를 반환합니다.

```text
listening fd
  -> accept()
  -> client fd
```

이 구분이 매우 중요합니다.

| fd | 설명 | 오래 살아 있는가 |
| --- | --- | --- |
| listening fd | 새 연결을 기다리는 socket | 서버가 켜져 있는 동안 유지 |
| client fd | 특정 클라이언트와 통신하는 socket | 요청 처리 후 닫음 |

그래서 “`accept()`가 없다면 서버는 클라이언트를 어떻게 구분할까?”라는 질문의 답은 자연스럽게 여기서 나옵니다. 구분할 수 없습니다. `accept()`가 반환하는 client fd가 있어야, 서버는 “이번 요청은 이 클라이언트 연결에서 온 것”이라고 다룰 수 있습니다.

이 프로젝트의 로그에서 보이는 `fd=4` 같은 값이 바로 이 client fd입니다.

```text
[thread=6134444032 fd=4] POST /query ok elapsed_us=4 index_used=true
```

fd는 운영체제가 프로세스에게 준 번호입니다. 프로세스는 그 번호로 `recv()`, `send()`, `close()`를 호출합니다. 파일도 fd로 다루고 socket도 fd로 다룬다는 점은 Unix 계열 OS의 강력한 추상화입니다.

### 최소구현의 선택
이 서버는 blocking `accept()`를 사용합니다. 연결이 올 때까지 main thread는 기다립니다. 실제 고성능 서버는 non-blocking I/O나 event notification을 사용하기도 하지만, 이 프로젝트는 thread pool 구조를 배우기 위해 blocking accept + worker pool을 선택했습니다.

## 3단계: main thread는 왜 요청을 직접 처리하지 않는가
`accept()`가 client fd를 반환하면, `server_run()`은 그 fd를 `thread_pool_submit()`에 넘깁니다.

```text
main thread
  -> accept()
  -> thread_pool_submit(client_fd)

worker thread
  -> worker_loop()
  -> handle_client(client_fd)
```

main thread가 직접 `handle_client()`를 호출해도 기능은 만들 수 있습니다. 하지만 그러면 한 요청을 처리하는 동안 새 연결을 받는 일이 늦어집니다. 특히 DB 작업이 오래 걸리거나 클라이언트가 느리면 서버 전체가 막힌 것처럼 보일 수 있습니다.

thread pool은 이 문제를 줄입니다. main thread는 연결 수락에 집중하고, worker thread는 실제 요청 처리에 집중합니다.

`src/thread_pool.c`는 bounded queue를 사용합니다.

| 구성요소 | 역할 |
| --- | --- |
| `fds[]` | 처리 대기 중인 client fd 배열 |
| `head`, `tail`, `count` | circular queue 상태 |
| `mutex` | queue 상태 보호 |
| `not_empty` | worker가 잠들었다가 fd가 들어오면 깨어남 |
| `not_full` | queue가 꽉 찼다가 자리가 나면 submit 쪽이 깨어남 |

여기서 mutex와 condition variable은 서로 다른 문제를 해결합니다. mutex는 동시에 queue 상태를 바꾸는 것을 막고, condition variable은 조건이 만족될 때까지 thread를 효율적으로 기다리게 합니다.

### 최소구현의 선택
queue가 가득 차면 submit 쪽은 기다립니다. 실제 서버에서는 요청을 거절하거나, timeout을 두거나, backpressure를 클라이언트에게 전달할 수도 있습니다. 이 프로젝트는 동시성 구조를 이해하기 위해 가장 단순하고 설명 가능한 방식을 택했습니다.

## 4단계: TCP byte stream은 어떻게 HTTP request가 되는가
worker가 `handle_client(client_fd)`를 실행하면 가장 먼저 `http_read_request()`를 호출합니다. 이 함수는 fd에서 byte를 읽어 `HttpRequest` 구조체를 채웁니다.

HTTP 요청은 텍스트처럼 보이지만, socket에서 읽을 때는 그냥 byte stream입니다. TCP는 순서와 신뢰성을 제공하지만 메시지 경계는 제공하지 않습니다. 그러므로 HTTP parser가 직접 경계를 찾아야 합니다.

```text
byte stream
  -> request line
  -> headers
  -> blank line
  -> body
```

이 프로젝트는 두 규칙을 사용합니다.

1. `\r\n\r\n`가 나오면 header가 끝난다.
2. `Content-Length`만큼 body를 더 읽는다.

그래서 `Content-Length`가 왜 중요한지도 이 단계에서 이해됩니다. body가 몇 byte인지 알아야 서버가 SQL JSON body를 어디까지 읽을지 판단할 수 있습니다. TCP 연결을 닫을 때까지 읽는 방식도 가능하지만, keep-alive 같은 기능이 들어오면 요청 하나의 끝을 알기 어렵습니다.

`HttpRequest`는 다음 정보를 담습니다.

| 필드 | 의미 |
| --- | --- |
| `method` | `GET`, `POST` 같은 HTTP method |
| `path` | `/health`, `/query` 같은 route |
| `body` | JSON body |
| `body_len` | body 길이 |

### 최소구현의 선택
이 parser는 HTTP 전체를 구현하지 않습니다. chunked transfer, keep-alive, header normalization, timeout, multi-request connection은 없습니다. 하지만 method, path, body를 얻는 핵심 구조가 드러나므로 학습에는 유리합니다.

## 5단계: route는 요청의 목적지를 정한다
`handle_client()`는 `HttpRequest`의 method와 path를 보고 분기합니다.

| 요청 | 처리 |
| --- | --- |
| `GET /health` | `{"status":"ok"}` 반환 |
| `POST /query` | SQL 처리 흐름으로 진입 |
| 그 외 path | 404 error JSON |
| 지원하지 않는 method | 405 error JSON |

route 분기는 네트워크 레이어와 DB 레이어 사이의 문지기 역할을 합니다. 모든 HTTP 요청이 SQL 실행으로 이어지지는 않습니다. `/health`는 DB를 건드리지 않고 서버가 살아 있는지만 확인합니다.

`POST /query`는 `handle_query()`로 이어집니다. 이 함수는 세 가지 일을 순서대로 합니다.

```text
JSON에서 SQL 추출
  -> SQL 파싱
  -> DB 실행
```

여기서 중요한 것은 실패도 정상적인 응답 경로를 가진다는 점입니다. JSON이 잘못되었거나 SQL 문법이 지원 범위를 벗어나면 서버는 죽지 않고 `{"ok":false,"error":"..."}` 형태로 응답합니다.

### 최소구현의 선택
route는 두 개뿐입니다. RESTful resource 설계나 인증/권한은 없습니다. 대신 API server가 요청을 받아 내부 engine으로 연결하는 최소 구조를 명확히 보여 줍니다.

## 6단계: JSON에서 SQL을 꺼내는 이유
`http_extract_sql()`은 body에서 `"sql"` key의 문자열 값을 꺼냅니다.

```json
{"sql":"SELECT * FROM users WHERE id = 1;"}
```

SQL을 body에 그냥 넣어도 학습 프로젝트는 동작할 수 있습니다. 하지만 JSON으로 감싸면 API 모양이 더 명확해지고, 나중에 다른 field를 추가하기 쉽습니다.

```json
{
  "sql": "SELECT * FROM users WHERE id = 1;",
  "debug": true,
  "request_id": "req-001"
}
```

물론 현재 구현은 완전한 JSON parser가 아닙니다. `"sql"` key의 문자열 값만 찾는 최소 구현입니다. 이 한계는 의도적입니다. 여기서 배우려는 핵심은 JSON 표준 전체가 아니라 **HTTP body에서 SQL 문자열을 다음 계층으로 넘기는 경계**입니다.

### 최소구현의 선택
실제 서비스에서는 검증된 JSON library를 사용해야 합니다. 직접 만든 parser는 edge case와 보안 입력에 취약해지기 쉽습니다.

## 7단계: SQL parser는 왜 필요한가
`sql_parse()`는 SQL 문자열을 `SqlStatement`로 변환합니다.

문자열:

```sql
SELECT * FROM users WHERE id = 1;
```

구조체 의미:

```text
type: SQL_SELECT
table: users
where_type: SQL_WHERE_ID
where_id: 1
```

이 변환은 단순한 형식 변경이 아닙니다. 사람이 읽는 문장을 프로그램이 실행하기 좋은 구조로 바꾸는 일입니다. DB engine은 SQL 문자열을 다시 해석하지 않고 `stmt->type`, `stmt->where_type` 같은 필드를 보고 실행합니다.

parser와 executor를 나누면 다음 장점이 생깁니다.

| 분리 전 | 분리 후 |
| --- | --- |
| DB engine이 문자열을 직접 분석해야 함 | DB engine은 구조화된 의도만 처리 |
| 문법 오류와 실행 오류가 섞임 | parsing 실패와 실행 실패를 구분 |
| 기능 추가 시 코드가 얽힘 | parser, executor를 나누어 확장 가능 |

`WHERE id = 1`과 `WHERE name = 'kim'`의 실행 경로 차이도 parser에서 시작됩니다. parser가 `SQL_WHERE_ID` 또는 `SQL_WHERE_NAME`으로 구분해 주기 때문에, DB engine이 id 조건일 때만 B+ tree를 사용할 수 있습니다.

### 최소구현의 선택
이 parser는 tokenizer, AST, query planner를 따로 두지 않습니다. 지원 문법이 작기 때문입니다. 실제 DBMS는 훨씬 복잡한 SQL을 처리하기 위해 여러 단계를 둡니다.

## 8단계: DB engine은 세 상태를 함께 유지한다
`src/db.c`는 SQL 실행의 중심입니다. 이 모듈은 세 가지 상태를 함께 관리합니다.

```text
CSV file
  persistent storage

Record array
  in-memory current state

B+ tree
  id index
```

### INSERT의 실제 의미
`INSERT INTO users name age VALUES 'kim' 20;`은 단지 파일에 한 줄 쓰는 일이 아닙니다.

```text
write lock 획득
  -> 새 id 부여
  -> CSV 파일 append
  -> records 배열에 Record 추가
  -> B+ tree에 id -> record index 등록
  -> 응답 JSON 생성
  -> write lock 해제
```

파일만 갱신하고 메모리 배열을 갱신하지 않으면, 서버가 실행 중인 동안 방금 넣은 record를 빠르게 조회할 수 없습니다. 반대로 메모리만 갱신하고 파일에 쓰지 않으면 서버를 재시작했을 때 데이터가 사라집니다.

### SELECT의 두 경로
`SELECT * FROM users WHERE id = 1;`은 B+ tree를 사용합니다.

```text
id
  -> bptree_search()
  -> record index
  -> records[record_index]
```

`SELECT * FROM users WHERE name = 'kim';`은 name index가 없으므로 선형 탐색합니다.

```text
records[0] 확인
  -> records[1] 확인
  -> records[2] 확인
  -> ...
```

여기서 `index_used`의 의미가 자연스럽게 드러납니다. `index_used:true`는 “이번 조회가 B+ tree index를 사용했다”는 뜻이고, `false`는 “index 없이 record 배열을 훑었다”는 뜻입니다.

### 최소구현의 선택
이 프로젝트는 index 자체를 파일에 저장하지 않습니다. 서버 시작 시 CSV를 읽고 B+ tree를 다시 만듭니다. 실제 DBMS는 index도 디스크에 저장하고 crash recovery까지 고려합니다.

## 9단계: 동시성은 DB 상태를 지키는 문제다
thread pool 덕분에 여러 worker가 동시에 `db_select()`나 `db_insert()`를 호출할 수 있습니다. 문제는 DB engine 내부 상태가 공유된다는 점입니다.

공유 상태:

- `records`
- `count`
- `capacity`
- `next_id`
- `index`
- CSV data file

SELECT는 읽기 작업입니다. 여러 SELECT가 동시에 같은 records 배열을 읽는 것은 괜찮습니다. 그래서 `db_select()`는 read lock을 잡습니다.

INSERT는 쓰기 작업입니다. 파일, 배열, B+ tree, next id를 모두 바꿉니다. 이 작업은 중간 상태를 노출하면 안 됩니다. 그래서 `db_insert()`는 write lock을 잡습니다.

| 작업 | lock | 동시 실행 가능성 |
| --- | --- | --- |
| SELECT + SELECT | read lock | 가능 |
| SELECT + INSERT | read/write 충돌 | 한쪽이 기다림 |
| INSERT + INSERT | write lock 충돌 | 하나씩 실행 |

이 구조는 단순하지만 강력합니다. 실제 DBMS처럼 row-level lock이나 MVCC를 구현하지 않아도, 공유 상태가 깨지는 대표적인 문제를 막을 수 있습니다.

### 최소구현의 선택
전역 read-write lock은 이해하기 쉽지만 확장성은 제한됩니다. 테이블이 많아지거나 긴 SELECT가 많아지면 INSERT가 기다릴 수 있습니다. 실제 DBMS는 더 세밀한 lock과 transaction isolation을 사용합니다.

## 10단계: 응답과 로그는 내부 실행을 밖으로 드러낸다
DB 실행 결과는 `DbResult`에 담깁니다.

| 필드 | 의미 |
| --- | --- |
| `ok` | 성공 여부 |
| `rows_json` | 결과 row 배열 JSON |
| `message` | 실행 메시지 |
| `index_used` | B+ tree 사용 여부 |
| `elapsed_us` | DB 실행 시간 |

서버는 이를 HTTP JSON response로 변환합니다.

```json
{"ok":true,"rows":[{"id":1,"name":"kim","age":20}],"message":"selected 1 row(s)","index_used":true,"elapsed_us":4}
```

`elapsed_us`는 성능을 정밀하게 보장하는 측정값이라기보다 학습 관찰값입니다. 데이터가 적으면 id 검색과 name 검색의 차이가 작아 보일 수 있습니다. 하지만 데이터가 커지고 반복 측정을 하면 index lookup과 linear scan의 경향 차이를 볼 수 있습니다.

로그도 관찰을 돕습니다.

```text
[thread=6134444032 fd=4] POST /query ok elapsed_us=4 index_used=true
```

여기서 `thread`는 어떤 worker가 처리했는지, `fd`는 어떤 클라이언트 연결이었는지, `index_used`는 DB 내부 경로가 무엇이었는지 보여 줍니다.

### 최소구현의 선택
학습용이라 응답에 내부 실행 정보를 넣었습니다. 실제 서비스에서는 내부 성능 정보와 구조를 사용자 응답에 그대로 노출하지 않고, 로그나 metrics 시스템으로 분리하는 경우가 많습니다.

## 이 문서에서 붙잡아야 할 큰 구조
```text
네트워크 계층
  socket, bind, listen, accept, fd

요청 처리 계층
  thread pool, HTTP parser, routing

의미 변환 계층
  JSON body -> SQL string -> SqlStatement

DB 실행 계층
  file storage, memory records, B+ tree, lock

응답 계층
  DbResult -> JSON -> HTTP response
```

각 계층은 자기 책임을 갖습니다. 이 책임 분리가 이 프로젝트를 읽는 가장 좋은 지도입니다.

## 여기까지 이해했는지 확인
- [ ] `accept()`가 listening fd와 client fd 사이에서 어떤 역할을 하는지 설명할 수 있다.
- [ ] `Content-Length`가 TCP byte stream 위에서 HTTP body를 읽는 데 왜 필요한지 설명할 수 있다.
- [ ] `SqlStatement`가 parser와 DB engine 사이의 계약이라는 점을 설명할 수 있다.
- [ ] `INSERT`가 파일, 메모리 배열, B+ tree를 모두 바꾼다는 점을 말할 수 있다.
- [ ] read-write lock이 SELECT와 INSERT를 다르게 다루는 이유를 설명할 수 있다.
- [ ] `index_used`와 `elapsed_us`가 학습 관찰값으로 어떤 의미를 갖는지 설명할 수 있다.

다음 문서: [03_code_reading/README.md](03_code_reading/README.md)
