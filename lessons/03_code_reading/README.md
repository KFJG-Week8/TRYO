# 03. 코드 병렬 독해 가이드

## 먼저 한 문장으로 보기
이 단계는 실제 코드를 옆에 열어두고 읽는 구간입니다. 하지만 코드를 파일 순서대로 읽지 않고, **요청 하나가 지나가는 순서**대로 읽습니다.

## 코드 독해 전에 붙잡을 원칙
코드는 세부 구현입니다. 세부 구현을 보기 전에 항상 다음 세 가지를 먼저 확인하세요.

| 질문 | 이유 |
| --- | --- |
| 이 함수는 어떤 계층에 속하는가? | socket 코드인지, HTTP 코드인지, DB 코드인지 알아야 세부 구현이 보입니다. |
| 이 함수는 무엇을 입력받아 무엇을 넘기는가? | 시스템은 데이터의 형태가 바뀌며 진행됩니다. |
| 이 함수가 실패하면 호출자는 어떻게 아는가? | C 프로젝트에서는 error handling이 설계의 일부입니다. |

## 추천 독해 순서
| 순서 | 문서 | 읽을 코드 | 핵심 관점 |
| --- | --- | --- | --- |
| 1 | [01_server_lifecycle.md](01_server_lifecycle.md) | `src/main.c`, `src/server.c` | 프로그램이 TCP 서버가 되는 과정 |
| 2 | [02_thread_pool_and_fd_queue.md](02_thread_pool_and_fd_queue.md) | `src/thread_pool.c`, `include/thread_pool.h` | client fd가 worker에게 넘어가는 과정 |
| 3 | [03_http_and_json.md](03_http_and_json.md) | `src/http.c`, `include/http.h` | byte stream을 HTTP 요청으로 해석하는 과정 |
| 4 | [04_sql_parser.md](04_sql_parser.md) | `src/sql.c`, `include/sql.h` | SQL 문자열을 구조체로 바꾸는 과정 |
| 5 | [05_db_engine.md](05_db_engine.md) | `src/db.c`, `include/db.h` | 파일, 메모리, lock, index가 함께 움직이는 과정 |
| 6 | [06_bptree_index.md](06_bptree_index.md) | `src/bptree.c`, `include/bptree.h` | id 검색 인덱스가 작동하는 과정 |

## API 카드 읽는 법
각 문서에는 함수별 API 카드가 있습니다. API 카드는 함수의 설명서가 아니라, 함수가 시스템 안에서 맡은 **역할 카드**입니다.

| 항목 | 보는 법 |
| --- | --- |
| 이름 | 코드를 검색할 때 사용할 정확한 함수명 |
| 위치 | 어느 파일에 있는지 |
| 한 문장 목적 | 이 함수가 없으면 어떤 흐름이 끊기는지 |
| 입력 | 어떤 데이터를 받는지 |
| 출력 | 호출자에게 무엇을 돌려주는지 |
| 호출되는 시점 | 요청 흐름 중 언제 등장하는지 |
| 내부에서 하는 일 | 세부 구현의 큰 단계 |
| 실패할 수 있는 지점 | C 코드에서 놓치기 쉬운 error path |
| 학습자가 확인할 질문 | 코드를 읽으며 붙잡을 관찰 지점 |

## 코드 관찰 방법
코드를 읽을 때는 다음 순서를 추천합니다.

1. 문서의 흐름도를 먼저 봅니다.
2. API 카드에서 함수의 목적과 입력/출력을 봅니다.
3. 실제 코드에서 함수 signature를 확인합니다.
4. 에러 처리 경로를 찾습니다.
5. 함수가 다음 계층에 무엇을 넘기는지 확인합니다.

## 흔한 오해
| 오해 | 바로잡기 |
| --- | --- |
| 작은 함수부터 완벽히 이해해야 전체가 보인다. | 전체 요청 흐름을 먼저 잡아야 작은 함수의 의미가 보입니다. |
| socket fd는 요청 데이터 자체다. | fd는 데이터를 읽고 쓸 수 있는 통로입니다. |
| HTTP parser와 SQL parser는 비슷한 parser다. | 둘 다 문자열을 해석하지만, 하나는 protocol framing이고 하나는 query language 해석입니다. |
| B+ tree는 record 전체를 저장한다. | 이 프로젝트의 B+ tree는 `id -> record index`만 저장합니다. |

다음: [01_server_lifecycle.md](01_server_lifecycle.md)
