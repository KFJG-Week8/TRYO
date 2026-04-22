# 03-05. DB Engine: 파일, 메모리, Lock으로 SQL 실행하기

## 먼저 한 문장으로 보기
`db.c`는 파싱된 SQL 의도를 실제 데이터 삽입과 조회로 바꾸며, 파일 저장소, 메모리 배열, B+ tree index, read-write lock을 함께 관리합니다.

## 요청 흐름에서의 위치
```text
SqlStatement
  -> execute_statement()
  -> db_insert() 또는 db_select()
  -> Record 배열, CSV 파일, B+ tree
  -> DbResult
```

## 이 코드를 읽기 전에 알아야 할 CS 개념
| 개념 | 짧은 설명 | 이 파일에서 보이는 지점 |
| --- | --- | --- |
| persistence | 프로그램이 종료되어도 데이터가 남게 하는 성질입니다. | CSV data file |
| in-memory state | 실행 중 빠른 접근을 위해 메모리에 유지하는 상태입니다. | `records` 배열 |
| consistency | 파일, 배열, 인덱스가 같은 데이터를 가리키는 성질입니다. | `db_insert()` |
| read-write lock | 읽기는 병렬로, 쓰기는 배타적으로 처리하는 lock입니다. | `pthread_rwlock_t` |
| serialization | record를 JSON 문자열로 바꾸는 작업입니다. | `append_record_json()` |

## 주요 구조체: `Record`
- 이름: `Record`
- 위치: `include/db.h`
- 한 문장 목적: `users` table의 row 하나를 표현합니다.
- 필드:
  - `id`: 자동 증가 정수
  - `name`: 사용자 이름
  - `age`: 나이
- 학습자가 확인할 질문:
  - table column이 추가되면 `Record`, parser, serializer는 어떻게 바뀌어야 할까요?

## 주요 구조체: `DbEngine`
- 이름: `DbEngine`
- 위치: `include/db.h`
- 한 문장 목적: DB 실행에 필요한 전체 상태를 묶어 보관합니다.
- 주요 필드:
  - `data_path`: CSV 파일 경로
  - `records`: 메모리 record 배열
  - `count`, `capacity`: 배열 사용량과 할당량
  - `next_id`: 다음 INSERT에 줄 id
  - `index`: B+ tree index
  - `lock`: read-write lock
- 학습자가 확인할 질문:
  - 왜 DB 상태를 전역 변수로 흩뿌리지 않고 구조체 하나로 묶었을까요?

## 주요 구조체: `DbResult`
- 이름: `DbResult`
- 위치: `include/db.h`
- 한 문장 목적: DB 실행 결과를 서버 레이어에 전달합니다.
- 주요 필드:
  - `ok`: 성공 여부
  - `rows_json`: row 결과를 담은 JSON 배열 문자열
  - `message`: 사람이 읽을 수 있는 메시지
  - `index_used`: B+ tree 사용 여부
  - `elapsed_us`: 실행 시간
- 학습자가 확인할 질문:
  - DB engine이 JSON을 만들어 주는 설계는 편하지만 어떤 한계가 있을까요?

## API 카드: `db_init`
- 이름: `db_init`
- 위치: `src/db.c`, 선언은 `include/db.h`
- 한 문장 목적: data file을 열고, 기존 record를 읽어 메모리 배열과 B+ tree index를 복구합니다.
- 입력:
  - `DbEngine *db`
  - `const char *data_path`
  - error buffer
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점: 서버 시작 시 `server_run()` 내부
- 내부에서 하는 일:
  - `DbEngine`을 0으로 초기화합니다.
  - `next_id`를 1로 둡니다.
  - read-write lock을 초기화합니다.
  - B+ tree를 초기화합니다.
  - data file을 `a+`로 열어 없으면 생성합니다.
  - 파일의 각 줄을 `Record`로 읽습니다.
  - record 배열에 넣고 B+ tree에 id를 등록합니다.
- 실패할 수 있는 지점:
  - data path가 너무 김
  - lock 초기화 실패
  - 파일 open 실패
  - CSV line이 잘못됨
  - 메모리 할당 실패
- 학습자가 확인할 질문:
  - 서버 시작 시 인덱스를 다시 만드는 방식은 어떤 장단점이 있을까요?

## API 카드: `db_insert`
- 이름: `db_insert`
- 위치: `src/db.c`
- 한 문장 목적: 새 record에 id를 부여하고 파일, 메모리 배열, B+ tree를 모두 갱신합니다.
- 입력: `DbEngine *db`, `const char *name`, `int age`
- 출력: `DbResult`
- 호출되는 시점: `execute_statement()`가 `SQL_INSERT`를 처리할 때
- 내부에서 하는 일:
  - write lock을 획득합니다.
  - name과 age를 검증합니다.
  - record 배열 capacity를 확보합니다.
  - `next_id`로 새 id를 정합니다.
  - CSV 파일에 한 줄 append합니다.
  - 메모리 배열에 record를 추가합니다.
  - B+ tree에 `id -> record_index`를 등록합니다.
  - 결과 row JSON을 만듭니다.
  - write lock을 해제합니다.
- 실패할 수 있는 지점:
  - write lock 획득 실패
  - 잘못된 name 또는 age
  - 메모리 부족
  - 파일 append 실패
  - B+ tree 갱신 실패
  - JSON 직렬화 실패
- 학습자가 확인할 질문:
  - 파일 append는 성공했는데 B+ tree 갱신이 실패하면 어떤 문제가 남을까요?

## API 카드: `db_select`
- 이름: `db_select`
- 위치: `src/db.c`
- 한 문장 목적: filter에 맞는 record를 찾고 JSON rows로 직렬화합니다.
- 입력: `DbEngine *db`, `DbFilter filter`
- 출력: `DbResult`
- 호출되는 시점: `execute_statement()`가 `SQL_SELECT`를 처리할 때
- 내부에서 하는 일:
  - read lock을 획득합니다.
  - filter가 `DB_FILTER_ID`면 B+ tree에서 record index를 찾습니다.
  - filter가 `DB_FILTER_NAME`이면 record 배열을 선형 탐색합니다.
  - filter가 `DB_FILTER_ALL`이면 전체 record를 순회합니다.
  - 결과 record들을 JSON 배열로 만듭니다.
  - `index_used`와 `elapsed_us`를 채웁니다.
  - read lock을 해제합니다.
- 실패할 수 있는 지점:
  - read lock 획득 실패
  - JSON 문자열 생성 실패
- 학습자가 확인할 질문:
  - 왜 `WHERE id`는 index를 쓰고 `WHERE name`은 선형 탐색을 할까요?

## API 카드: `db_destroy`
- 이름: `db_destroy`
- 위치: `src/db.c`
- 한 문장 목적: DB engine이 소유한 메모리와 동기화 자원을 정리합니다.
- 입력: `DbEngine *db`
- 출력: 없음
- 호출되는 시점: 서버 종료 또는 초기화 실패 처리 중
- 내부에서 하는 일:
  - record 배열을 해제합니다.
  - B+ tree를 해제합니다.
  - read-write lock을 destroy합니다.
- 실패할 수 있는 지점:
  - 현재 구현은 별도 실패 값을 반환하지 않습니다.
- 학습자가 확인할 질문:
  - C에서 `destroy` 함수가 필요한 이유는 무엇일까요?

## API 카드: `db_result_free`
- 이름: `db_result_free`
- 위치: `src/db.c`
- 한 문장 목적: `DbResult`가 동적으로 소유한 `rows_json` 메모리를 해제합니다.
- 입력: `DbResult *result`
- 출력: 없음
- 호출되는 시점: 서버가 response body를 만든 뒤
- 내부에서 하는 일:
  - `free(result->rows_json)`을 호출합니다.
  - 포인터를 `NULL`로 바꿉니다.
- 실패할 수 있는 지점:
  - 없음
- 학습자가 확인할 질문:
  - 결과 구조체 자체가 stack에 있어도 내부 문자열은 왜 따로 free해야 할까요?

## Read-write lock의 의미
`pthread_rwlock_t`는 읽기와 쓰기를 다르게 취급합니다.

- read lock: 여러 SELECT가 동시에 잡을 수 있습니다.
- write lock: INSERT가 혼자 잡아야 합니다.

INSERT는 파일, 메모리 배열, B+ tree를 모두 바꾸므로 중간 상태를 다른 thread가 보면 안 됩니다. 반면 SELECT는 상태를 바꾸지 않으므로 여러 thread가 동시에 읽어도 됩니다.

## 파일 저장과 메모리 상태의 차이
파일은 서버가 꺼져도 남습니다. 메모리 배열은 서버가 실행 중일 때만 존재합니다.

그래서 `db_init()`이 중요합니다. 서버 시작 시 파일을 읽어 메모리 배열과 B+ tree를 다시 만듭니다. 이 과정을 통해 영속 저장소와 실행 중 상태가 연결됩니다.

## 코드 관찰 포인트
- `db_init()`은 파일을 여는 것에서 끝나지 않고, 기존 데이터를 읽어 메모리 배열과 B+ tree를 복구합니다.
- `db_insert()`는 write lock 안에서 파일, 배열, index를 순서대로 갱신합니다.
- `db_select()`는 filter 종류에 따라 B+ tree 검색과 선형 탐색으로 갈라집니다.
- `DbResult`는 DB 결과와 학습용 관찰 정보(`index_used`, `elapsed_us`)를 함께 담습니다.

## 흔한 오해
| 오해 | 바로잡기 |
| --- | --- |
| 파일에 저장하면 메모리 배열은 필요 없다. | 매 조회마다 파일을 읽지 않으려면 실행 중 상태가 필요합니다. |
| read lock은 lock이 아니므로 안전성과 무관하다. | read lock도 동시성 계약의 일부입니다. write lock과 충돌해 중간 상태 읽기를 막습니다. |
| JSON 생성은 서버 레이어에서만 해야 한다. | 현재 구현은 단순화를 위해 DB result가 row JSON을 갖습니다. 실제 시스템에서는 분리할 수도 있습니다. |

## 다음 문서로 넘어가기
이제 `WHERE id` 조회를 빠르게 만드는 B+ tree index를 살펴봅니다.

다음: [06_bptree_index.md](06_bptree_index.md)
