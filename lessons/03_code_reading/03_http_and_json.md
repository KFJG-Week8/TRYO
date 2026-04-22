# 03-03. HTTP와 JSON: Byte Stream을 요청으로 해석하기

## 먼저 한 문장으로 보기
`http.c`는 client fd에서 TCP byte stream을 읽어 HTTP 요청 구조로 바꾸고, JSON HTTP 응답을 다시 byte stream으로 써 주는 모듈입니다.

## 요청 흐름에서의 위치
```text
worker가 client_fd를 받음
  -> http_read_request()
  -> HttpRequest { method, path, body }
  -> route 분기
  -> http_extract_sql()
  -> SQL 문자열
  -> http_send_json()
```

## 이 코드를 읽기 전에 알아야 할 CS 개념
| 개념 | 짧은 설명 | 이 파일에서 보이는 지점 |
| --- | --- | --- |
| byte stream | TCP는 메시지가 아니라 순서 있는 byte 흐름을 제공합니다. | `recv()` 반복 |
| message framing | byte 흐름에서 메시지 경계를 찾는 일입니다. | `\r\n\r\n`, `Content-Length` |
| partial read/write | 한 번의 `recv()`나 `send()`가 전체 데이터를 처리한다는 보장은 없습니다. | `http_read_request()`, `send_all()` |
| serialization | 구조화된 결과를 전송 가능한 문자열로 만드는 일입니다. | `http_send_json()` |
| minimal parser | 전체 표준이 아니라 필요한 부분만 해석합니다. | `http_extract_sql()` |

## TCP는 메시지 경계를 모른다
HTTP 요청은 사람이 보기에는 하나의 메시지처럼 보입니다.

```http
POST /query HTTP/1.1
Content-Length: 45

{"sql":"SELECT * FROM users WHERE id = 1;"}
```

하지만 TCP 입장에서는 이것이 그냥 byte들의 흐름입니다. 한 번의 `recv()`가 HTTP 요청 전체를 가져온다는 보장이 없습니다. 그래서 `http_read_request()`는 header 끝을 찾고, `Content-Length`만큼 body가 올 때까지 계속 읽습니다.

## API 카드: `http_read_request`
- 이름: `http_read_request`
- 위치: `src/http.c`, 선언은 `include/http.h`
- 한 문장 목적: client fd에서 HTTP request를 읽어 `HttpRequest` 구조체로 채웁니다.
- 입력:
  - `int client_fd`
  - `HttpRequest *request`
  - `char *err`
  - `size_t err_size`
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점: `handle_client()` 시작부
- 내부에서 하는 일:
  - `recv()`로 byte를 읽습니다.
  - `\r\n\r\n`를 찾아 header 끝을 판단합니다.
  - request line에서 method와 path를 읽습니다.
  - `Content-Length`를 찾습니다.
  - body 길이만큼 추가로 읽습니다.
  - body를 `request->body`에 복사합니다.
- 실패할 수 있는 지점:
  - client가 연결을 끊음
  - header가 너무 큼
  - request line 형식이 이상함
  - `Content-Length`가 잘못됨
  - body가 너무 큼
- 학습자가 확인할 질문:
  - 왜 header 끝을 찾은 뒤에도 body를 더 읽어야 할 수 있을까요?
  - `HTTP_MAX_REQUEST`, `HTTP_MAX_BODY` 같은 제한값은 왜 필요할까요?

## API 카드: `http_send_json`
- 이름: `http_send_json`
- 위치: `src/http.c`
- 한 문장 목적: JSON 문자열을 HTTP response 형식으로 감싸 client fd에 씁니다.
- 입력:
  - `int client_fd`
  - `int status_code`
  - `const char *json_body`
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점: `/health`, `/query`, error response를 보낼 때
- 내부에서 하는 일:
  - status code에 맞는 reason phrase를 고릅니다.
  - `Content-Type`, `Content-Length`, `Connection: close` header를 만듭니다.
  - header와 body를 `send_all()`로 전송합니다.
- 실패할 수 있는 지점:
  - header 문자열 생성 실패
  - `send()` 실패
- 학습자가 확인할 질문:
  - 응답에서도 `Content-Length`가 필요한 이유는 무엇일까요?
  - 왜 `Connection: close`를 명시했을까요?

## API 카드: `send_all`
- 이름: `send_all`
- 위치: `src/http.c`
- 한 문장 목적: `send()`가 일부 byte만 보낼 수 있으므로, 전체 buffer가 전송될 때까지 반복합니다.
- 입력: fd, buffer, 길이
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점: `http_send_json()` 내부
- 내부에서 하는 일:
  - 이미 보낸 byte 수를 추적합니다.
  - 남은 byte를 `send()`로 보냅니다.
  - `EINTR`이면 다시 시도합니다.
- 실패할 수 있는 지점:
  - 연결이 끊김
  - `send()` 에러
- 학습자가 확인할 질문:
  - 왜 `send()` 한 번으로 충분하다고 가정하면 안 될까요?

## API 카드: `http_extract_sql`
- 이름: `http_extract_sql`
- 위치: `src/http.c`
- 한 문장 목적: JSON body에서 `"sql"` key의 문자열 값을 꺼냅니다.
- 입력:
  - `const char *body`
  - `char *sql_out`
  - `size_t sql_size`
  - `char *err`
  - `size_t err_size`
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점: `handle_query()`에서 HTTP request body를 얻은 뒤
- 내부에서 하는 일:
  - body에서 `"sql"` 문자열을 찾습니다.
  - `:` 다음의 JSON string 값을 읽습니다.
  - 일부 escape 문자만 처리합니다.
  - 결과 SQL을 `sql_out`에 씁니다.
- 실패할 수 있는 지점:
  - `sql` key가 없음
  - `sql` 값이 문자열이 아님
  - 지원하지 않는 escape 문자가 있음
  - SQL 문자열이 너무 김
- 학습자가 확인할 질문:
  - 이 함수가 완전한 JSON parser가 아닌 이유는 무엇일까요?
  - 학습용 최소 구현과 production parser의 차이는 어디서 시작될까요?

## 이 프로젝트의 JSON parser는 의도적으로 작다
현재 구현은 `{"sql":"..."}` 형태를 처리하기 위한 최소 기능만 갖습니다. 배열, 중첩 object, unicode escape, 엄격한 JSON validation을 모두 구현하지 않습니다.

이것은 약점이기도 하지만 학습 관점에서는 장점입니다. 목적은 JSON 표준 전체를 구현하는 것이 아니라, “HTTP body에서 SQL 문자열을 꺼내 다음 단계로 넘긴다”는 흐름을 이해하는 것입니다.

## 코드 관찰 포인트
- `http_read_request()`는 header 끝을 먼저 찾고, 그 뒤 `Content-Length`만큼 body를 보장하려고 다시 읽습니다.
- `send_all()`은 `send()`가 일부만 성공할 수 있다는 네트워크 프로그래밍의 현실을 반영합니다.
- `http_extract_sql()`은 SQL 문법을 보지 않습니다. JSON body에서 문자열만 꺼냅니다.

## 흔한 오해
| 오해 | 바로잡기 |
| --- | --- |
| TCP가 요청 단위를 알아서 나눠 준다. | TCP는 byte stream만 제공합니다. HTTP parser가 요청 경계를 찾아야 합니다. |
| `Content-Type: application/json`이면 body가 안전한 JSON이다. | header는 선언일 뿐입니다. body 검증은 별도입니다. |
| 작은 프로젝트에서는 partial write를 신경 쓰지 않아도 된다. | 학습 단계에서도 `send_all()` 패턴을 익히는 것이 좋습니다. |

## 다음 문서로 넘어가기
이제 SQL 문자열이 구조화된 실행 의도로 바뀌는 과정을 봅니다.

다음: [04_sql_parser.md](04_sql_parser.md)
