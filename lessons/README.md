# WEEK8 미니 DBMS API 서버 Top-Down 학습서

## 이 학습서의 목적
이 학습서는 C로 구현한 미니 DBMS API 서버를 통해 **네트워크, 운영체제, HTTP, 동시성, SQL 처리, 파일 저장, B+ 트리 인덱스**를 한 번에 연결해 보는 학습 지도입니다.

입문자를 대상으로 하지만, 목표 수준을 낮게 잡지 않습니다. “이 코드는 이렇게 생겼다”에서 멈추지 않고, 다음 질문에 자기 말로 답할 수 있게 만드는 것이 목표입니다.

- TCP 연결은 서버 코드에서 어떤 fd로 나타나는가?
- HTTP 요청은 TCP byte stream 위에서 어떻게 구분되는가?
- SQL 문자열은 왜 바로 실행하지 않고 구조체로 바꾸는가?
- 파일 저장소와 메모리 상태는 왜 둘 다 필요한가?
- 여러 thread가 동시에 요청을 처리할 때 DB 상태는 어떻게 보호되는가?
- B+ 트리 인덱스는 왜 `WHERE id = ?` 검색을 빠르게 만드는가?

## Top-Down으로 읽는다는 것
코드를 처음부터 끝까지 순서대로 읽으면, 입문자는 보통 두 번 길을 잃습니다.

첫째, 작은 함수의 세부 구현에 빠져 전체 요청 흐름을 놓칩니다. 둘째, socket, fd, thread, parser, index 같은 개념이 따로 흩어져 보입니다.

이 학습서는 반대로 읽습니다.

```text
큰 그림
  -> 요청 하나가 지나가는 구조
  -> 각 파일과 함수의 책임
  -> 더 깊은 CS 질문
```

항상 다음 흐름을 기준으로 생각하세요.

```text
curl client
  -> TCP 연결
  -> accept()로 client fd 생성
  -> thread pool queue에 fd 전달
  -> worker thread가 HTTP request 읽기
  -> JSON body에서 SQL 추출
  -> SQL parser가 SqlStatement 생성
  -> DB engine이 INSERT/SELECT 실행
  -> 파일, 메모리 배열, B+ tree 사용
  -> JSON HTTP response 전송
  -> client fd close
```

## 문서별 역할
| 문서 | 역할 | 읽고 나면 답해야 할 질문 |
| --- | --- | --- |
| [00_top_down_analysis_koh.md](00_top_down_analysis_koh.md) | 이번 정리에서 추가한 탑다운 분석 문서입니다. | SQL 처리기와 B+ 트리 이후 무엇이 추가되어 실제 서버 흐름이 되었는가? |
| [01_big_picture.md](01_big_picture.md) | 프로젝트 전체를 CS 개념 지도처럼 훑습니다. | 이 프로젝트는 어떤 계층들을 통과하며 요청을 처리하는가? |
| [02_architecture_walkthrough.md](02_architecture_walkthrough.md) | 요청 하나를 따라가며 구조와 원리를 풍부하게 설명합니다. | 각 파일은 무엇을 입력받고 무엇을 다음 단계로 넘기는가? |
| [03_code_reading/README.md](03_code_reading/README.md) | 코드와 나란히 읽는 기술 문서의 입구입니다. | 어떤 순서로 코드를 읽어야 전체 흐름을 잃지 않는가? |
| [04_expansion_questions.md](04_expansion_questions.md) | 질문 블록을 통해 사고를 확장합니다. | 이 최소구현을 실제 시스템으로 키우려면 무엇을 고민해야 하는가? |

## 질문 카드 읽는 법
문서에는 다음 형식의 질문 카드가 반복됩니다.

> **질문. 왜 서버는 `accept()`가 반환한 fd를 worker에게 넘길까?**
>
> **배경:** `accept()`는 새 TCP 연결을 받아 client fd를 만듭니다. 이 fd는 특정 클라이언트와 통신하는 통로입니다.
>
> **답변을 찾는 방향:** main thread가 요청 처리까지 직접 하면 새 연결을 받지 못하고 막힐 수 있습니다. 그래서 fd를 worker에게 넘겨 역할을 나눕니다.

질문을 시험 문제처럼 보지 마세요. 질문은 다음 층으로 내려가는 손잡이입니다. 질문을 읽고 바로 코드를 찾기보다, 먼저 “이 질문이 어느 계층을 바라보게 하는가”를 생각하세요.

## 실행하며 관찰할 값
서버를 켜고 요청을 보내면 응답과 로그에서 학습에 유용한 값이 나옵니다.

| 값 | 어디서 보나 | 의미 |
| --- | --- | --- |
| `fd` | 서버 로그 | 운영체제가 client 연결에 부여한 file descriptor |
| `thread` | 서버 로그 | 어떤 worker thread가 요청을 처리했는지 |
| `index_used` | JSON 응답, 로그 | 이번 SELECT가 B+ tree index를 사용했는지 |
| `elapsed_us` | JSON 응답, 로그 | DB 실행에 걸린 시간, microsecond 단위 |
| `rows` | JSON 응답 | SQL 실행 결과 row 배열 |

예를 들어 `SELECT * FROM users WHERE id = 1;`은 B+ tree를 사용하므로 `index_used:true`가 됩니다. 반대로 `SELECT * FROM users WHERE name = 'kim';`은 name index가 없으므로 record 배열을 선형 탐색하고 `index_used:false`가 됩니다.

## 실행 예시
```sh
make
./bin/week8_dbms
```

다른 터미널에서:

```sh
curl http://127.0.0.1:8080/health
```

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data '{"sql":"INSERT INTO users name age VALUES '\''kim'\'' 20;"}'
```

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data '{"sql":"SELECT * FROM users WHERE id = 1;"}'
```

## 학습 체크리스트
- [ ] 요청 하나가 `curl`에서 `db_select()`까지 이동하는 큰 흐름을 말할 수 있다.
- [ ] listening fd와 client fd의 차이를 설명할 수 있다.
- [ ] TCP byte stream과 HTTP message의 차이를 설명할 수 있다.
- [ ] SQL parser와 DB engine의 책임을 구분할 수 있다.
- [ ] file storage, memory records, B+ tree index가 각각 왜 필요한지 설명할 수 있다.
- [ ] thread pool과 read-write lock이 해결하는 문제가 서로 다르다는 것을 설명할 수 있다.

다음 문서: [00_top_down_analysis_koh.md](00_top_down_analysis_koh.md)
