# 01. 큰 그림: 요청 하나로 배우는 네트워크, OS, DB

## 먼저 한 문장으로 보기
이 프로젝트는 **HTTP API 서버**와 **미니 DBMS**를 하나로 묶은 학습용 시스템입니다. 클라이언트가 SQL을 HTTP로 보내면, 서버는 TCP 연결을 받아 요청을 읽고, SQL을 해석하고, 파일 기반 DB와 B+ 트리 인덱스를 사용해 결과를 JSON으로 응답합니다.

## 전체 그림
```text
클라이언트
  |
  | HTTP request over TCP
  v
서버 socket
  |
  | accept() -> client fd
  v
thread pool
  |
  | worker가 fd 처리
  v
HTTP parser
  |
  | body에서 {"sql":"..."} 추출
  v
SQL parser
  |
  | SqlStatement 생성
  v
DB engine
  |
  | file + memory records + B+ tree
  v
HTTP JSON response
```

이 그림에서 중요한 점은, 요청이 이동할 때마다 **형태가 바뀐다**는 것입니다.

| 단계 | 데이터의 모습 | 핵심 질문 |
| --- | --- | --- |
| 네트워크 | TCP byte stream | 연결은 어떻게 만들어지는가? |
| HTTP | method, path, headers, body | byte stream에서 요청 하나를 어떻게 구분하는가? |
| JSON | `{"sql":"..."}` | body 안에서 SQL 문자열을 어떻게 찾는가? |
| SQL parser | `SqlStatement` | 문자열을 실행 의도로 어떻게 바꾸는가? |
| DB engine | `Record`, `DbFilter`, `DbResult` | 실제 데이터는 어디서 읽고 쓰는가? |
| 응답 | JSON over HTTP | 결과와 관찰 정보를 어떻게 돌려주는가? |

## 먼저 알아둘 관찰 값
이 프로젝트는 학습을 돕기 위해 응답과 로그에 실행 정보를 넣었습니다.

| 이름 | 뜻 | 왜 중요한가 |
| --- | --- | --- |
| `index_used` | 이번 DB 조회가 B+ tree index를 사용했는지 나타내는 boolean 값 | 같은 SELECT라도 index lookup과 linear scan의 실행 경로가 다름을 관찰하게 해 줍니다. |
| `elapsed_us` | DB 실행에 걸린 시간, microsecond 단위 | 데이터가 많아질 때 검색 방식에 따라 시간이 달라지는지 볼 수 있습니다. |
| `fd` | file descriptor. 열린 파일이나 socket을 가리키는 정수 | OS가 네트워크 연결을 프로그램에 어떻게 표현하는지 보여 줍니다. |
| `thread` | 요청을 처리한 worker thread의 id | thread pool이 실제로 여러 worker에게 일을 나누는지 보여 줍니다. |

예를 들어 id 조건 조회는 다음처럼 응답합니다.

```json
{"ok":true,"rows":[{"id":1,"name":"kim","age":20}],"message":"selected 1 row(s)","index_used":true,"elapsed_us":4}
```

여기서 `index_used:true`는 “이 요청은 B+ tree를 통해 record 위치를 찾았다”는 뜻입니다. `elapsed_us:4`는 “DB 실행 구간이 약 4 microsecond 걸렸다”는 뜻입니다. 숫자 자체보다 중요한 것은, 이 값들이 **실행 경로를 관찰할 수 있는 창**이라는 점입니다.

## 1단계: 접속, TCP, socket, file descriptor
클라이언트가 `127.0.0.1:8080`으로 요청을 보내면 서버는 TCP 연결을 받아야 합니다. C 코드에서는 BSD socket API가 이 일을 맡습니다.

핵심 흐름은 다음과 같습니다.

```text
socket() -> bind() -> listen() -> accept()
```

**socket**은 네트워크 통신을 위한 운영체제 자원입니다. 프로그램은 socket을 직접 만지는 것이 아니라 **file descriptor(fd)** 라는 정수를 통해 접근합니다. 서버에는 두 종류의 fd가 등장합니다.

| fd 종류 | 역할 |
| --- | --- |
| listening fd | 새 클라이언트 연결을 기다리는 socket |
| client fd | 특정 클라이언트 한 명과 통신하는 socket |

`accept()`가 중요한 이유는 listening fd에서 실제 client fd를 만들어 주기 때문입니다. 서버가 여러 클라이언트를 구분할 수 있는 것도 이 client fd 덕분입니다.

이 프로젝트에서는 `src/server.c`가 이 부분을 담당합니다.

> **질문 1. 왜 `socket()`만 호출하면 서버가 완성되지 않을까?**
>
> **배경:** `socket()`은 통신용 자원을 만들 뿐입니다. 아직 어떤 port에서 기다릴지, 연결 대기 상태인지 정해지지 않았습니다.
>
> **답변을 찾는 방향:** `bind()`와 `listen()`이 각각 어떤 상태 변화를 만드는지 생각해 보세요.

> **질문 2. listening fd와 client fd를 구분하지 않으면 어떤 혼란이 생길까?**
>
> **배경:** listening fd는 새 연결을 기다리고, client fd는 이미 연결된 클라이언트와 통신합니다.
>
> **답변을 찾는 방향:** `accept()`가 반환한 fd를 worker에게 넘기는 이유를 떠올려 보세요.

> **질문 3. file descriptor가 단순한 정수라면, 왜 그렇게 강력한 추상화일까?**
>
> **배경:** fd는 파일, socket 같은 서로 다른 입출력 대상을 비슷한 방식으로 다루게 해 줍니다.
>
> **답변을 찾는 방향:** `close(client_fd)`가 네트워크 연결을 닫는다는 사실을 파일 close와 비교해 보세요.

> **질문 4. IP와 port는 각각 무엇을 식별할까?**
>
> **배경:** `127.0.0.1`은 이 컴퓨터 자신을 가리키는 loopback IP이고, `8080`은 같은 컴퓨터 안의 특정 서버 프로그램을 찾는 번호입니다.
>
> **답변을 찾는 방향:** 같은 컴퓨터에서 여러 서버가 동시에 실행될 수 있는 이유를 port 관점에서 생각해 보세요.

## 2단계: HTTP 요청 읽기와 message framing
TCP는 순서 있는 byte stream을 제공합니다. 하지만 TCP는 “여기부터 여기까지가 HTTP 요청 하나”라고 알려주지 않습니다. HTTP가 그 위에 메시지 형식을 정합니다.

HTTP 요청은 대략 이런 모양입니다.

```http
POST /query HTTP/1.1
Content-Length: 45

{"sql":"SELECT * FROM users WHERE id = 1;"}
```

서버는 두 가지 단서를 사용합니다.

| 단서 | 의미 |
| --- | --- |
| `\r\n\r\n` | HTTP header가 끝나는 위치 |
| `Content-Length` | body가 몇 byte인지 알려주는 header |

이 프로젝트에서는 `src/http.c`의 `http_read_request()`가 이 일을 합니다. 이 parser는 학습용 최소구현이라 method, path, `Content-Length`, body만 처리합니다.

> **질문 1. 왜 한 번의 `recv()`가 요청 전체를 가져온다고 가정하면 안 될까?**
>
> **배경:** TCP는 byte stream입니다. 네트워크와 OS buffer 상태에 따라 데이터가 여러 번에 나뉘어 도착할 수 있습니다.
>
> **답변을 찾는 방향:** `http_read_request()`가 header 끝과 body 길이를 따로 확인하는 이유를 생각해 보세요.

> **질문 2. `Content-Length`가 없다면 서버는 body 끝을 어떻게 알 수 있을까?**
>
> **배경:** HTTP/1.1에서 body 길이를 알 수 없으면 서버는 요청이 끝났는지 판단하기 어렵습니다.
>
> **답변을 찾는 방향:** 현재 구현이 keep-alive를 지원하지 않고 `Connection: close`를 쓰는 이유와 연결해 보세요.

> **질문 3. HTTP parser가 완전하지 않다는 것은 어떤 뜻일까?**
>
> **배경:** 실제 HTTP에는 chunked transfer, keep-alive, 다양한 header, malformed request 처리, timeout 등이 있습니다.
>
> **답변을 찾는 방향:** 이 프로젝트가 왜 그런 기능을 의도적으로 제외했는지 학습 목표와 연결해 보세요.

> **질문 4. HTTP는 왜 TCP 위에 따로 필요한 약속일까?**
>
> **배경:** TCP는 byte 전달을 담당하고, HTTP는 그 byte를 요청과 응답이라는 의미 있는 단위로 해석합니다.
>
> **답변을 찾는 방향:** 같은 TCP 연결 위에서도 HTTP, Redis protocol, custom protocol이 모두 가능하다는 점을 떠올려 보세요.

## 3단계: JSON body에서 SQL 추출
`POST /query` 요청의 body는 JSON입니다.

```json
{"sql":"SELECT * FROM users WHERE id = 1;"}
```

JSON은 key-value 형식의 텍스트 데이터 표현입니다. 이 프로젝트에서는 완전한 JSON parser를 만들지 않고, `"sql"` key의 문자열 값만 꺼냅니다. 이 일은 `src/http.c`의 `http_extract_sql()`이 담당합니다.

왜 SQL만 body에 바로 보내지 않고 JSON으로 감쌀까요? 지금은 `sql` 하나만 필요하지만, API는 보통 시간이 지나며 확장됩니다.

```json
{"sql":"SELECT * FROM users WHERE id = 1;","debug":true,"request_id":"abc"}
```

JSON을 사용하면 나중에 이런 정보를 함께 담을 수 있습니다.

> **질문 1. 왜 API 요청 body를 JSON으로 만들면 확장성이 좋아질까?**
>
> **배경:** 단순 문자열 body는 SQL 하나만 담기 쉽지만, JSON은 여러 field를 함께 담을 수 있습니다.
>
> **답변을 찾는 방향:** `sql` 외에 timeout, trace id, debug option이 필요해지는 상황을 상상해 보세요.

> **질문 2. 이 프로젝트의 JSON parser가 production에서 위험한 이유는 무엇일까?**
>
> **배경:** 현재 구현은 `"sql"` 문자열 추출만 처리하고, JSON 표준 전체를 검증하지 않습니다.
>
> **답변을 찾는 방향:** unicode escape, nested object, 잘못된 JSON, 매우 큰 body 같은 입력을 생각해 보세요.

> **질문 3. SQL 문자열을 외부에서 직접 받는 API는 어떤 위험을 만들까?**
>
> **배경:** 실제 서비스에서 사용자 입력 SQL을 그대로 실행하면 권한, 데이터 노출, SQL injection 문제가 생길 수 있습니다.
>
> **답변을 찾는 방향:** 학습 프로젝트와 실제 서비스의 보안 요구가 어떻게 다른지 구분해 보세요.

> **질문 4. JSON 추출은 HTTP parser의 책임일까, SQL parser의 책임일까?**
>
> **배경:** HTTP parser는 body를 읽고, SQL parser는 SQL 문법을 해석합니다. JSON은 그 사이에 있습니다.
>
> **답변을 찾는 방향:** 레이어별 책임을 나누면 테스트와 교체가 왜 쉬워지는지 생각해 보세요.

## 4단계: SQL parser가 문자열을 구조체로 바꾼다
SQL은 사람이 읽기 좋은 문자열입니다. 하지만 DB engine이 매번 긴 문자열을 직접 다루면 복잡하고 위험합니다. 그래서 parser가 SQL을 `SqlStatement` 구조체로 바꿉니다.

예를 들어:

```sql
SELECT * FROM users WHERE id = 1;
```

이 SQL은 다음 의미로 구조화됩니다.

```text
type: SQL_SELECT
table: users
where_type: SQL_WHERE_ID
where_id: 1
```

이 프로젝트의 SQL parser는 `src/sql.c`에 있고, 결과 구조체는 `include/sql.h`의 `SqlStatement`입니다.

지원 문법은 작습니다.

```sql
INSERT INTO users name age VALUES 'kim' 20;
SELECT * FROM users;
SELECT * FROM users WHERE id = 1;
SELECT * FROM users WHERE name = 'kim';
```

> **질문 1. 왜 SQL 문자열을 그대로 DB engine에 넘기지 않을까?**
>
> **배경:** 문자열은 의미가 분해되어 있지 않습니다. DB engine은 “SELECT인지 INSERT인지”, “조건 column이 무엇인지”를 명시적으로 알아야 합니다.
>
> **답변을 찾는 방향:** `SqlStatement`의 필드가 DB engine의 분기를 어떻게 단순하게 만드는지 보세요.

> **질문 2. parser가 지원 문법을 작게 제한하면 무엇을 얻을까?**
>
> **배경:** SQL 전체 문법은 매우 큽니다. 입문 단계에서 전부 구현하면 핵심 흐름이 가려집니다.
>
> **답변을 찾는 방향:** 최소 문법이 “파싱 -> 실행” 구조를 더 선명하게 보여 준다는 점을 생각해 보세요.

> **질문 3. `WHERE id = 1`과 `WHERE name = 'kim'`은 왜 실행 경로가 다를까?**
>
> **배경:** parser는 두 조건을 서로 다른 `where_type`으로 구조화합니다. DB engine은 이 정보를 보고 index 사용 여부를 결정합니다.
>
> **답변을 찾는 방향:** `SQL_WHERE_ID`가 `DB_FILTER_ID`로 이어지는 흐름을 떠올려 보세요.

> **질문 4. 실제 DBMS의 parser는 여기서 얼마나 더 복잡해질까?**
>
> **배경:** 실제 SQL에는 JOIN, ORDER BY, GROUP BY, subquery, function, transaction 문법이 있습니다.
>
> **답변을 찾는 방향:** 단순 구조체 하나가 아니라 token, AST, planner가 필요해지는 이유를 생각해 보세요.

## 5단계: DB engine, 파일 저장, 메모리 배열, B+ tree
DB engine은 파싱된 SQL 의도를 실제 데이터 작업으로 바꿉니다. 이 프로젝트의 DB 상태는 세 가지로 나뉩니다.

| 상태 | 역할 |
| --- | --- |
| CSV 파일 | 서버가 꺼져도 데이터가 남는 영속 저장소 |
| 메모리 `Record` 배열 | 실행 중 빠르게 조회하기 위한 현재 데이터 상태 |
| B+ tree index | `id`로 record 위치를 빠르게 찾기 위한 인덱스 |

`INSERT`는 세 곳을 모두 갱신합니다.

```text
파일 append
  -> 메모리 배열에 Record 추가
  -> B+ tree에 id -> record index 등록
```

`SELECT WHERE id = N`은 B+ tree를 사용합니다. `SELECT WHERE name = 'kim'`은 name index가 없으므로 메모리 배열을 처음부터 끝까지 봅니다.

동시성도 여기서 중요해집니다. 여러 worker thread가 동시에 DB engine을 호출할 수 있기 때문입니다. 이 프로젝트는 `pthread_rwlock_t`를 사용합니다.

| 작업 | lock | 이유 |
| --- | --- | --- |
| SELECT | read lock | 데이터를 바꾸지 않으므로 여러 thread가 동시에 읽어도 됩니다. |
| INSERT | write lock | 파일, 배열, 인덱스를 모두 바꾸므로 혼자 실행해야 합니다. |

> **질문 1. 왜 파일에만 저장하지 않고 메모리 배열도 유지할까?**
>
> **배경:** 파일은 오래 남지만 매번 검색하기엔 느립니다. 메모리는 빠르지만 서버가 꺼지면 사라집니다.
>
> **답변을 찾는 방향:** `db_init()`이 서버 시작 시 파일을 읽어 메모리 상태를 복구하는 이유를 생각해 보세요.

> **질문 2. 왜 `id` 검색에는 B+ tree를 쓰고 `name` 검색에는 쓰지 않을까?**
>
> **배경:** 현재 index는 `id -> record index`만 저장합니다. name에 대한 index는 없습니다.
>
> **답변을 찾는 방향:** index가 있는 column과 없는 column의 검색 경로 차이를 비교해 보세요.

> **질문 3. INSERT 중간에 다른 SELECT가 데이터를 읽으면 어떤 문제가 생길 수 있을까?**
>
> **배경:** INSERT는 파일, 메모리 배열, B+ tree를 순서대로 갱신합니다. 중간 상태를 읽으면 불일치가 보일 수 있습니다.
>
> **답변을 찾는 방향:** write lock이 어떤 구간을 하나의 안전한 작업처럼 보호하는지 생각해 보세요.

> **질문 4. 서버가 재시작되면 B+ tree는 어디서 다시 만들어질까?**
>
> **배경:** B+ tree는 메모리에만 있습니다. 파일에는 record만 저장됩니다.
>
> **답변을 찾는 방향:** `db_init()`이 CSV 파일을 읽으며 `bptree_insert()`를 호출하는 장면을 상상해 보세요.

## 6단계: 응답, 로그, 관찰 가능성
서버는 DB 결과를 JSON HTTP 응답으로 보냅니다.

```json
{
  "ok": true,
  "rows": [{"id":1,"name":"kim","age":20}],
  "message": "selected 1 row(s)",
  "index_used": true,
  "elapsed_us": 4
}
```

응답은 단순히 데이터를 돌려주는 역할만 하지 않습니다. 이 프로젝트에서는 학습자가 내부 실행 경로를 관찰할 수 있도록 `index_used`와 `elapsed_us`를 포함합니다.

서버 로그도 마찬가지입니다.

```text
[thread=6134444032 fd=4] POST /query ok elapsed_us=4 index_used=true
```

이 로그를 통해 “어떤 worker가 어떤 fd를 처리했고, index를 썼는지”를 볼 수 있습니다.

> **질문 1. 왜 `index_used`는 학습에 중요한 관찰 값일까?**
>
> **배경:** DB 내부 실행 경로는 밖에서 보이지 않습니다. `index_used`는 이번 요청이 B+ tree를 사용했는지 알려 줍니다.
>
> **답변을 찾는 방향:** 같은 SELECT라도 id 조건과 name 조건의 응답을 비교해 보세요.

> **질문 2. `elapsed_us`는 정확한 성능 측정값일까, 관찰용 힌트일까?**
>
> **배경:** microbenchmark는 OS 스케줄링, 캐시, 데이터 크기, 실행 환경의 영향을 받습니다.
>
> **답변을 찾는 방향:** 숫자 하나보다 여러 번 실행했을 때의 경향을 보는 것이 왜 중요한지 생각해 보세요.

> **질문 3. production API에서도 내부 성능 정보를 응답에 그대로 넣어도 될까?**
>
> **배경:** 학습용으로는 좋지만 실제 서비스에서는 내부 구조 노출이나 보안 문제가 될 수 있습니다.
>
> **답변을 찾는 방향:** 사용자에게 필요한 정보와 운영자가 로그에서 봐야 할 정보를 구분해 보세요.

> **질문 4. 로그의 `thread`와 `fd`를 보면 어떤 시스템 동작을 추적할 수 있을까?**
>
> **배경:** thread pool과 socket fd는 눈에 보이지 않는 실행 흐름입니다. 로그는 그 흐름을 밖으로 드러냅니다.
>
> **답변을 찾는 방향:** 여러 요청을 동시에 보냈을 때 thread id가 어떻게 달라지는지 관찰해 보세요.

## 여기까지 이해했는지 확인
- [ ] TCP, HTTP, JSON, SQL, DB engine이 서로 다른 계층이라는 것을 설명할 수 있다.
- [ ] listening fd와 client fd의 차이를 말할 수 있다.
- [ ] `Content-Length`가 HTTP body를 읽는 데 왜 필요한지 설명할 수 있다.
- [ ] `SqlStatement`가 왜 필요한지 설명할 수 있다.
- [ ] `index_used:true`와 `index_used:false`가 어떤 실행 경로 차이를 뜻하는지 말할 수 있다.
- [ ] read lock과 write lock이 각각 어떤 상황에서 쓰이는지 설명할 수 있다.

다음 문서: [02_architecture_walkthrough.md](02_architecture_walkthrough.md)
