# 03-01. 서버 생명주기와 요청 라우팅

## 먼저 한 문장으로 보기
`main.c`와 `server.c`는 프로그램을 TCP 서버로 만들고, client fd를 받아 HTTP route에 따라 요청을 처리하는 가장 바깥 레이어입니다.

## 요청 흐름에서의 위치
```text
프로그램 실행
  -> main.c
  -> server_run()
  -> create_listen_socket()
  -> accept()
  -> thread_pool_submit()
  -> worker에서 handle_client()
```

## 이 코드를 읽기 전에 알아야 할 CS 개념
| 개념 | 짧은 설명 | 이 파일에서 보이는 지점 |
| --- | --- | --- |
| process entry point | C 프로그램은 `main()`에서 시작합니다. | `src/main.c` |
| socket lifecycle | TCP 서버는 socket을 만들고 port에 묶고 listen 상태가 됩니다. | `create_listen_socket()` |
| file descriptor | 열린 socket을 프로세스가 다루는 정수 번호입니다. | `listen_fd`, `client_fd` |
| routing | HTTP method/path에 따라 처리 함수를 고릅니다. | `handle_client()` |
| orchestration | 여러 모듈을 순서대로 호출해 하나의 요청을 완성합니다. | `handle_query()` |

## API 카드: `server_run`
- 이름: `server_run`
- 위치: `src/server.c`, 선언은 `include/server.h`
- 한 문장 목적: DB와 thread pool을 초기화한 뒤 TCP 서버 accept loop를 실행합니다.
- 입력: `const ServerConfig *config`
- 출력: 성공 시 `0`, 실패 시 `1`
- 호출되는 시점: `main()`이 실행 인자를 정리한 직후
- 내부에서 하는 일:
  - signal handler를 설치합니다.
  - `db_init()`으로 DB engine을 준비합니다.
  - `thread_pool_init()`으로 worker thread를 만듭니다.
  - `create_listen_socket()`으로 listening socket을 만듭니다.
  - `accept()`로 client fd를 받고 thread pool queue에 넣습니다.
- 실패할 수 있는 지점:
  - data file을 열 수 없는 경우
  - thread 생성 실패
  - socket, bind, listen 실패
- 학습자가 확인할 질문:
  - 왜 DB 초기화가 socket 생성보다 먼저 일어날까요?
  - accept loop가 직접 `handle_client()`를 호출하지 않는 이유는 무엇일까요?

## API 카드: `create_listen_socket`
- 이름: `create_listen_socket`
- 위치: `src/server.c`
- 한 문장 목적: TCP 연결을 받을 준비가 된 listening socket fd를 만듭니다.
- 입력: `int port`
- 출력: 성공 시 listening fd, 실패 시 `-1`
- 호출되는 시점: `server_run()` 내부에서 accept loop 시작 전
- 내부에서 하는 일:
  - `socket(AF_INET, SOCK_STREAM, 0)`으로 TCP socket을 만듭니다.
  - `setsockopt(... SO_REUSEADDR ...)`로 port 재사용 옵션을 설정합니다.
  - `bind()`로 IP와 port에 socket을 묶습니다.
  - `listen()`으로 연결 대기 상태를 만듭니다.
- 실패할 수 있는 지점:
  - port 사용 중
  - 권한 문제
  - socket 생성 실패
- 학습자가 확인할 질문:
  - `listen_fd`와 `client_fd`는 어떤 점에서 다를까요?
  - `htonl`, `htons`는 왜 필요할까요?

## 핵심 코드블럭: `socket -> setsockopt -> bind -> listen -> accept`
이 순서는 BSD socket 서버의 기본 골격입니다.

```text
socket: 통신용 fd 생성
setsockopt: fd의 동작 옵션 설정
bind: fd를 port에 연결
listen: fd를 연결 대기 상태로 전환
accept: 실제 클라이언트 연결 하나를 fd로 받음
```

처음에는 이 함수들을 완벽히 외울 필요가 없습니다. 대신 “서버가 그냥 존재하는 것이 아니라, 운영체제에게 단계적으로 네트워크 자원을 요청한다”는 점을 붙잡으세요.

## API 카드: `handle_client`
- 이름: `handle_client`
- 위치: `src/server.c`
- 한 문장 목적: client fd 하나에 대해 HTTP request를 읽고 route에 맞는 handler를 호출합니다.
- 입력: `int client_fd`, `void *context`
- 출력: 없음. 응답 전송 후 fd를 닫습니다.
- 호출되는 시점: worker thread가 queue에서 client fd를 꺼낸 뒤
- 내부에서 하는 일:
  - `http_read_request()`로 HTTP 요청을 읽습니다.
  - `GET /health`면 상태 JSON을 보냅니다.
  - `POST /query`면 `handle_query()`로 넘깁니다.
  - 알 수 없는 method/path면 error JSON을 보냅니다.
  - 마지막에 `close(client_fd)`를 호출합니다.
- 실패할 수 있는 지점:
  - HTTP request가 잘못된 경우
  - client가 중간에 연결을 끊은 경우
- 학습자가 확인할 질문:
  - 왜 모든 요청 처리 후 fd를 닫을까요?
  - route 분기가 DB engine보다 바깥에 있는 이유는 무엇일까요?

## API 카드: `handle_query`
- 이름: `handle_query`
- 위치: `src/server.c`
- 한 문장 목적: `/query` 요청의 JSON body에서 SQL을 꺼내 파싱하고 DB 실행 결과를 응답합니다.
- 입력: `client_fd`, `ServerContext *context`, `HttpRequest *request`
- 출력: 없음. HTTP response를 fd에 씁니다.
- 호출되는 시점: `handle_client()`가 `POST /query`를 확인한 뒤
- 내부에서 하는 일:
  - `http_extract_sql()`로 JSON body에서 SQL 문자열을 추출합니다.
  - `sql_parse()`로 SQL을 `SqlStatement`로 바꿉니다.
  - `execute_statement()`로 DB 실행 함수를 호출합니다.
  - 성공하면 `make_success_body()`로 JSON response를 만듭니다.
  - 실패하면 `send_error_response()`를 호출합니다.
- 실패할 수 있는 지점:
  - JSON body에 `sql`이 없는 경우
  - SQL 문법이 지원 범위를 벗어난 경우
  - DB insert/select 중 에러가 난 경우
- 학습자가 확인할 질문:
  - 이 함수는 왜 SQL을 직접 실행하지 않고 `execute_statement()`를 부를까요?
  - 에러가 발생해도 JSON으로 응답하는 이유는 무엇일까요?

## API 카드: `execute_statement`
- 이름: `execute_statement`
- 위치: `src/server.c`
- 한 문장 목적: `SqlStatement`를 DB engine이 이해하는 함수 호출로 연결합니다.
- 입력: `DbEngine *db`, `const SqlStatement *stmt`
- 출력: `DbResult`
- 호출되는 시점: `handle_query()`에서 SQL parse가 성공한 뒤
- 내부에서 하는 일:
  - `SQL_INSERT`면 `db_insert()`를 호출합니다.
  - `SQL_SELECT`면 `where_type`에 따라 `DbFilter`를 만들고 `db_select()`를 호출합니다.
- 실패할 수 있는 지점:
  - 실제 실패는 주로 `db_insert()` 또는 `db_select()`에서 발생합니다.
- 학습자가 확인할 질문:
  - `SqlStatement`와 `DbFilter`는 각각 어떤 레이어의 언어일까요?

## route 분기
이 프로젝트의 route는 두 개입니다.

- `GET /health`: 서버 생존 확인
- `POST /query`: SQL 실행

이렇게 route를 작게 유지하면 네트워크 서버 구조를 흐리지 않고 볼 수 있습니다. 실제 API 서버라면 endpoint가 많아지고, handler도 파일별로 분리됩니다.

## 성공/실패 JSON response
성공 응답은 `ok`, `rows`, `message`, `index_used`, `elapsed_us`를 포함합니다. 실패 응답은 `ok:false`와 `error`를 포함합니다.

이 형식 덕분에 클라이언트는 HTTP status뿐 아니라 body에서도 성공과 실패 이유를 읽을 수 있습니다.

## 요청 로그 읽기
로그 예시:

```text
[thread=6134444032 fd=4] POST /query ok elapsed_us=4 index_used=true
```

- `thread`: 어떤 worker가 처리했는지
- `fd`: 어떤 client file descriptor였는지
- `elapsed_us`: DB 실행에 걸린 시간
- `index_used`: B+ tree index를 탔는지

이 로그는 학습용 관찰 장치입니다. 동시에 여러 요청을 보내면 서로 다른 worker thread가 등장하는지 확인할 수 있습니다.

## 코드 관찰 포인트
- `server_run()`은 직접 SQL을 파싱하지 않습니다. 서버의 바깥 생명주기와 모듈 연결을 담당합니다.
- `create_listen_socket()`에서 만들어지는 fd와 `accept()`가 반환하는 fd는 역할이 다릅니다.
- `handle_client()`는 route를 나누고, `handle_query()`는 SQL 처리 흐름을 시작합니다.
- `make_success_body()`와 `make_error_body()`는 DB 결과를 HTTP 응답 body로 직렬화하는 경계입니다.

## 흔한 오해
| 오해 | 바로잡기 |
| --- | --- |
| `socket()`을 호출하면 바로 클라이언트와 연결된다. | 서버 socket은 `bind()`와 `listen()`을 거쳐 연결 대기 상태가 되고, 실제 연결은 `accept()`에서 client fd로 나옵니다. |
| main thread가 모든 일을 처리하는 편이 단순하다. | 기능은 단순해질 수 있지만, 한 요청이 막히면 새 연결 수락도 지연됩니다. |
| route 분기는 사소한 if문이다. | route 분기는 네트워크 요청을 내부 기능으로 연결하는 API boundary입니다. |

## 다음 문서로 넘어가기
이제 client fd가 어떻게 worker에게 넘어가는지 봅니다.

다음: [02_thread_pool_and_fd_queue.md](02_thread_pool_and_fd_queue.md)
