# 현재 브랜치 기준 프로젝트 평가서

## 평가 전제

- 평가 대상: `D:\Dprojects\TRYO`
- 기준 요구사항: 6주차 SQL 처리기, 7주차 B+ 트리 인덱스, 8주차 미니 DBMS API 서버
- 평가 방식: 현재 브랜치의 코드, README, 테스트 코드, 보조 스크립트 정적 분석
- 검증 한계: 이번 세션에서는 `gcc`, `make`가 없고 `bash`도 접근 거부되어 실제 빌드/실행 테스트는 하지 못했다. 따라서 일부 판정은 실행 기반이 아니라 코드 기반 추정이다.

## 총평

현재 브랜치의 프로젝트는 이전에 봤던 버전보다 분명히 발전해 있다.  
특히 다음 기능이 추가되거나 더 뚜렷해졌다.

- `INSERT INTO users VALUES (id, 'name', age);` 형태 지원
- `SELECT id, name FROM users;` 같은 column projection 지원
- raw SQL body와 JSON body 둘 다 수용
- 명시적 ID insert와 auto-increment insert를 함께 지원

하지만 요구사항을 엄밀하게 대조하면, 여전히 **“8주차 중심 학습용 미니 서버”는 맞지만 6~8주차 전체 요구사항을 완전 충족했다고 보기는 어렵다.**

핵심 이유는 세 가지다.

1. 6주차의 핵심인 **CLI SQL 처리기**가 없다.
2. 7주차의 핵심인 **100만 건 이상 성능 실험과 정량 비교**가 부족하다.
3. 8주차의 서버 구조는 갖췄지만 **견고성, 이식성, 문서 품질**은 아직 학습용 수준이다.

## 주차별 평가

| 주차 | 판정 | 설명 |
| --- | --- | --- |
| 6주차 | 부분 충족 | SQL 파싱/실행/파일 저장은 있으나 CLI 파일 입력 처리기 없음 |
| 7주차 | 부분 충족 | B+Tree 인덱스와 ID 검색은 구현, 100만 건 성능 검증은 미흡 |
| 8주차 | 대체로 충족 | API 서버, thread pool, 동시성 제어, DB 엔진 연동은 구현 |

## 잘된 점

- 모듈 분리가 비교적 명확하다.
- SQL parser, DB engine, B+Tree, HTTP, thread pool 흐름이 연결되어 있다.
- `WHERE id = ?`는 인덱스, `WHERE name = ?`는 선형 탐색으로 분기된다.
- projection 기능이 있어 단순 `SELECT *`보다 한 단계 발전한 구조다.
- raw SQL과 JSON SQL을 모두 받아 데모 유연성이 좋아졌다.
- `pthread_rwlock_t`를 사용해 읽기/쓰기 동시성을 설명하기 좋다.

## 아쉬운 점

- 6주차 요구였던 CLI 처리기가 없고 서버 실행만 가능하다.
- 7주차의 대량 데이터 성능 실험이 제출물 수준으로 충분히 입증되지 않는다.
- `users` 단일 테이블 하드코딩이 강하다.
- HTTP/JSON 처리가 최소 구현이라 경계 입력에 취약하다.
- 파일 append 후 인덱스 갱신 실패 시 rollback이 없다.
- Windows 환경에서 바로 빌드하기 어려운 POSIX 전제가 강하다.
- 현재 브랜치의 README/requirements는 인코딩이 깨져 보이는 구간이 있어 문서 UX가 나쁘다.

## 200개 테스트 케이스와 판정

판정 기준:

- `PASS`: 현재 코드 기준으로 충족한다고 판단
- `PARTIAL`: 일부 충족 또는 조건부 충족
- `FAIL`: 요구사항 대비 부족하거나 미구현
- `UNVERIFIED`: 실제 실행 검증 불가

### A. 빌드, 실행, 환경, 문서 기본 (1-20)

| ID | 영역 | 테스트 케이스 | 기대 결과 | 판정 | 메모 |
| --- | --- | --- | --- | --- | --- |
| 1 | 빌드 | POSIX 환경에서 `make` 빌드 | 바이너리 생성 | UNVERIFIED | Makefile 존재 |
| 2 | 빌드 | `make test` 실행 | 테스트 바이너리 실행 | UNVERIFIED | 타깃 존재 |
| 3 | 빌드 | Windows PowerShell 단독 빌드 | 쉽게 빌드 | FAIL | POSIX 헤더, `mkdir -p`, `rm -rf` |
| 4 | 실행 | 인자 없이 서버 실행 | 기본 설정 사용 | PASS | `src/main.c` |
| 5 | 실행 | 포트 인자 지정 | 지정 포트 반영 | PASS | `argv[1]` |
| 6 | 실행 | thread 수 인자 지정 | 지정 thread 수 반영 | PASS | `argv[2]` |
| 7 | 실행 | 데이터 파일 경로 지정 | 지정 파일 사용 | PASS | `argv[3]` |
| 8 | 실행 | 잘못된 포트 문자열 입력 | 기본값 fallback | PASS | `parse_int_arg()` |
| 9 | 실행 | 잘못된 thread 수 입력 | 기본값 fallback | PASS | 동일 |
| 10 | 실행 | 잘못된 data path 입력 | DB init 실패 | PASS | `db_init()` 에러 |
| 11 | 요구 | SQL 파일을 CLI로 전달 | 처리 가능 | FAIL | 전용 CLI 처리기 없음 |
| 12 | 요구 | stdin SQL 입력 처리 | 가능 | FAIL | 없음 |
| 13 | 요구 | SQL 파일 여러 문장 순차 실행 | 가능 | FAIL | 없음 |
| 14 | 요구 | `--sql` 옵션 단건 실행 | 가능 | FAIL | 없음 |
| 15 | 문서 | README만으로 실행 방법 이해 | 가능 | PARTIAL | 내용은 있으나 인코딩 문제 |
| 16 | 문서 | README 인코딩 정상 표시 | 가독성 좋음 | FAIL | 현재 세션에서 깨져 보임 |
| 17 | 문서 | requirements 문서 인코딩 정상 | 가독성 좋음 | FAIL | 동일 |
| 18 | 문서 | 설계 문서 존재 | 있음 | PASS | `design.md` |
| 19 | 문서 | 발표 보조 문서 존재 | 있음 | PASS | `demo_guide.md`, `questions.md` |
| 20 | 종합 | 6주차~8주차 전체 가이드로 충분 | 충분 | PARTIAL | 8주차 중심 문서가 강함 |

### B. SQL 파서 기본 기능 (21-40)

| ID | 영역 | 테스트 케이스 | 기대 결과 | 판정 | 메모 |
| --- | --- | --- | --- | --- | --- |
| 21 | 파싱 | `INSERT INTO users name age VALUES 'kim' 20;` | 성공 | PASS | 기존 테스트 포함 |
| 22 | 파싱 | `INSERT INTO users VALUES (1, 'bumsang', 25);` | 성공 | PASS | 추가 기능 |
| 23 | 파싱 | `SELECT * FROM users;` | 성공 | PASS | 구현 존재 |
| 24 | 파싱 | `SELECT id, name FROM users;` | 성공 | PASS | projection 지원 |
| 25 | 파싱 | `SELECT name, id FROM users;` | 성공 | PASS | 순서 유지 |
| 26 | 파싱 | `SELECT * FROM users WHERE id = 42;` | 성공 | PASS | 구현 존재 |
| 27 | 파싱 | `SELECT * FROM users WHERE name = 'lee';` | 성공 | PASS | 구현 존재 |
| 28 | 파싱 | 소문자 SQL | 성공 | PASS | `strcasecmp` 사용 |
| 29 | 파싱 | 앞뒤 공백 포함 SQL | 성공 | PASS | `skip_ws()` |
| 30 | 파싱 | 세미콜론 없는 SQL | 성공 | PASS | `at_statement_end()` |
| 31 | 파싱 | 빈 SQL | 실패 | PASS | `empty SQL` |
| 32 | 파싱 | 공백만 있는 SQL | 실패 | PASS | 동일 |
| 33 | 파싱 | `DELETE FROM users` | 실패 | PASS | 미지원 |
| 34 | 파싱 | `UPDATE users` | 실패 | PASS | 미지원 |
| 35 | 파싱 | 다른 테이블명 `orders` | 실패 | PASS | `users`만 지원 |
| 36 | 파싱 | `SELECT age FROM users;` | 성공 | PASS | 단일 컬럼 가능 |
| 37 | 파싱 | `SELECT id, age FROM users;` | 성공 | PASS | 컬럼 조합 가능 |
| 38 | 파싱 | `SELECT id, id FROM users;` | 실패 | PASS | 중복 컬럼 차단 |
| 39 | 파싱 | `SELECT id, name, age FROM users;` | 성공 | PASS | 최대 3개 |
| 40 | 파싱 | `SELECT id, name, age, id FROM users;` | 실패 | PASS | 최대 컬럼 수 제한 |

### C. SQL 파서 경계 입력과 문법 제한 (41-60)

| ID | 영역 | 테스트 케이스 | 기대 결과 | 판정 | 메모 |
| --- | --- | --- | --- | --- | --- |
| 41 | 파싱 | `SELECT * FROM users WHERE age = 20;` | 실패 | PASS | `id/name`만 지원 |
| 42 | 파싱 | `SELECT * FROM users WHERE id = -1;` | 실패 | PASS | 음수 차단 |
| 43 | 파싱 | `INSERT ... VALUES (-1, 'kim', 20);` | 실패 | PASS | positive id 요구 |
| 44 | 파싱 | `INSERT ... VALUES (1, 'kim', -1);` | 실패 | PASS | 음수 age 차단 |
| 45 | 파싱 | `INSERT INTO users (name, age) VALUES ('kim', 20);` | 성공 기대 | FAIL | 표준 문법 미지원 |
| 46 | 파싱 | `INSERT INTO users(id, name, age) VALUES ...` | 성공 기대 | FAIL | 표준 컬럼 리스트 미지원 |
| 47 | 파싱 | `WHERE id=1` | 성공 | PASS | `=` 주변 공백 불필요 |
| 48 | 파싱 | `WHERE name='kim'` | 성공 | PASS | 동일 |
| 49 | 파싱 | `WHERE name = "kim"` | 실패 | PASS | 작은따옴표만 허용 |
| 50 | 파싱 | 이름에 공백 `'kim min'` | 성공 | PASS | 문자열 리터럴 |
| 51 | 파싱 | 이름 빈 문자열 `''` | 파싱 성공 | PASS | DB 레이어에서 거절 |
| 52 | 파싱 | 이름에 쉼표 `'kim,lee'` | 파싱 성공 | PASS | DB에서 거절 |
| 53 | 파싱 | 이름에 작은따옴표 포함 | 지원 | FAIL | escape 미지원 |
| 54 | 파싱 | trailing token 추가 | 실패 | PASS | trailing 검사 |
| 55 | 파싱 | `SELECT FROM users;` | 실패 | PASS | 컬럼 파싱 실패 |
| 56 | 파싱 | `INSERT INTO users VALUES ();` | 실패 | PASS | 값 부족 |
| 57 | 파싱 | `INSERT INTO users VALUES (1 'kim', 20);` | 실패 | PASS | comma 필수 |
| 58 | 파싱 | `SELECT *, name FROM users;` | 실패 | PASS | 혼합 미지원 |
| 59 | 파싱 | `SELECT id name FROM users;` | 실패 | PASS | comma 필수 |
| 60 | 파싱 | 복합 WHERE `id=1 AND name='x'` | 실패 | PASS | 미지원 |

### D. HTTP 입력 처리와 API 요청 파싱 (61-80)

| ID | 영역 | 테스트 케이스 | 기대 결과 | 판정 | 메모 |
| --- | --- | --- | --- | --- | --- |
| 61 | HTTP | raw SQL body 전달 | 성공 | PASS | `copy_raw_sql()` |
| 62 | HTTP | JSON `{ "sql": "..." }` 전달 | 성공 | PASS | `extract_json_sql()` |
| 63 | HTTP | raw body 앞뒤 공백 | trim 후 성공 | PASS | 구현 존재 |
| 64 | HTTP | raw body 빈 문자열 | 실패 | PASS | `SQL body is empty` |
| 65 | HTTP | JSON에 `sql` 키 없음 | 실패 | PASS | 기존 테스트 포함 |
| 66 | HTTP | 잘못된 JSON escape | 실패 | PASS | 구현 존재 |
| 67 | HTTP | `\"` escape 포함 JSON SQL | 성공 | PASS | 일부 escape 지원 |
| 68 | HTTP | `\n` escape 포함 JSON SQL | 성공 | PASS | 지원 |
| 69 | HTTP | `\uXXXX` escape 포함 JSON SQL | 성공 기대 | FAIL | 미지원 |
| 70 | HTTP | body가 `{`로 시작하는 raw SQL | raw로 처리 | FAIL | JSON로 오인 가능 |
| 71 | HTTP | nested JSON 구조 | 정확 파싱 | FAIL | 진짜 JSON parser 아님 |
| 72 | HTTP | `sql` 문자열이 다른 필드 값에 먼저 등장 | 정확 파싱 | FAIL | substring 기반 |
| 73 | HTTP | `Content-Length` 헤더 정상 POST | 성공 | PASS | 구현 존재 |
| 74 | HTTP | `Content-Length` 숫자 아님 | 실패 | PASS | 검사 존재 |
| 75 | HTTP | 헤더 과대 | 실패 | PASS | size 제한 |
| 76 | HTTP | body 과대 | 실패 | PASS | `HTTP_MAX_BODY` 검사 |
| 77 | HTTP | request line 깨짐 | 실패 | PASS | `sscanf` 검사 |
| 78 | HTTP | GET `/health` | 성공 | PASS | 구현 존재 |
| 79 | HTTP | POST `/query` | 성공 | PASS | 구현 존재 |
| 80 | HTTP | 없는 route | 404 | PASS | 구현 존재 |

### E. API 동작과 응답 형식 (81-100)

| ID | 영역 | 테스트 케이스 | 기대 결과 | 판정 | 메모 |
| --- | --- | --- | --- | --- | --- |
| 81 | API | `/health` 응답 JSON | `{"status":"ok"}` | PASS | 구현 존재 |
| 82 | API | `/query` 잘못된 method | 405 | PASS | 구현 존재 |
| 83 | API | `/health` 잘못된 method | 405 | PASS | 구현 존재 |
| 84 | API | 성공 응답에 `ok:true` | 포함 | PASS | `make_success_body()` |
| 85 | API | 실패 응답에 `ok:false` | 포함 | PASS | `make_error_body()` |
| 86 | API | 성공 응답에 `rows` | 포함 | PASS | 동일 |
| 87 | API | 성공 응답에 `message` | 포함 | PASS | 동일 |
| 88 | API | 성공 응답에 `index_used` | 포함 | PASS | 동일 |
| 89 | API | 성공 응답에 `elapsed_us` | 포함 | PASS | 동일 |
| 90 | API | 에러 시 문자열 escape 안전 | 대체로 안전 | PASS | JSON builder 사용 |
| 91 | API | INSERT raw SQL 요청 | 성공 | PASS | 구조상 가능 |
| 92 | API | SELECT projection raw SQL 요청 | 성공 | PASS | 구조상 가능 |
| 93 | API | SELECT JSON SQL 요청 | 성공 | PASS | 구조상 가능 |
| 94 | API | 중복 ID insert 요청 | 에러 응답 | PASS | `duplicate id` |
| 95 | API | 잘못된 SQL 요청 | 400 | PASS | parser 에러 |
| 96 | API | DB 에러 요청 | 500 | PASS | DB result 분기 |
| 97 | API | Content-Type 검증 | 엄격히 JSON/텍스트 구분 | FAIL | 헤더 검사 없음 |
| 98 | API | keep-alive 지원 | 연결 유지 | FAIL | close 방식 |
| 99 | API | HTTPS 지원 | 보안 연결 | FAIL | 미구현 |
| 100 | API | chunked body 지원 | 가능 | FAIL | 미구현 |

### F. DB insert, select, 저장 구조 (101-120)

| ID | 영역 | 테스트 케이스 | 기대 결과 | 판정 | 메모 |
| --- | --- | --- | --- | --- | --- |
| 101 | DB | auto increment insert | 성공 | PASS | `db_insert()` |
| 102 | DB | explicit id insert | 성공 | PASS | `db_insert_with_id()` |
| 103 | DB | duplicate explicit id insert | 거절 | PASS | B+Tree search |
| 104 | DB | auto id 이후 explicit 큰 id insert | next_id 갱신 | PASS | `record.id >= next_id` |
| 105 | DB | explicit 작은 id insert 후 next_id 유지 | 유지 | PASS | 조건부 갱신 |
| 106 | DB | 음수 age insert | 거절 | PASS | validation |
| 107 | DB | 빈 name insert | 거절 | PASS | validation |
| 108 | DB | 쉼표 포함 name insert | 거절 | PASS | CSV 보호 |
| 109 | DB | 줄바꿈 포함 name insert | 거절 | PASS | CSV 보호 |
| 110 | DB | 파일 append 성공 | 한 줄 추가 | PASS | `fprintf` |
| 111 | DB | SELECT ALL | 전건 반환 | PASS | `DB_FILTER_ALL` |
| 112 | DB | SELECT WHERE id | 한 건 반환 | PASS | index 경로 |
| 113 | DB | SELECT WHERE name | 매칭 목록 반환 | PASS | scan 경로 |
| 114 | DB | 없는 id 조회 | 빈 배열 | PASS | 검색 실패 시 `[]` |
| 115 | DB | 없는 name 조회 | 빈 배열 | PASS | scan 결과 없음 |
| 116 | DB | 재기동 후 데이터 재로딩 | 성공 | PASS | 테스트 코드 포함 |
| 117 | DB | malformed CSV 파일 | init 실패 | PASS | `db_init()` 검사 |
| 118 | DB | negative age 포함 파일 | init 실패 | PASS | 유효성 검사 |
| 119 | DB | 음수 id 포함 파일 | init 실패 | PASS | 유효성 검사 |
| 120 | DB | 테이블별 파일 분리 | 여러 테이블 지원 | FAIL | 단일 users 고정 |

### G. SELECT projection과 결과 직렬화 (121-140)

| ID | 영역 | 테스트 케이스 | 기대 결과 | 판정 | 메모 |
| --- | --- | --- | --- | --- | --- |
| 121 | Projection | `SELECT id FROM users;` | id만 반환 | PASS | 구조상 가능 |
| 122 | Projection | `SELECT name FROM users;` | name만 반환 | PASS | 구조상 가능 |
| 123 | Projection | `SELECT age FROM users;` | age만 반환 | PASS | 구조상 가능 |
| 124 | Projection | `SELECT id, name FROM users;` | 두 필드만 반환 | PASS | 테스트 포함 |
| 125 | Projection | `SELECT name, id FROM users;` | 순서 유지 | PASS | 테스트 포함 |
| 126 | Projection | `SELECT id, age FROM users;` | 순서 유지 | PASS | 구현상 가능 |
| 127 | Projection | `SELECT name, age FROM users;` | 순서 유지 | PASS | 구현상 가능 |
| 128 | Projection | `SELECT id, name, age FROM users;` | 전체 반환 | PASS | 구현상 가능 |
| 129 | Projection | `SELECT * FROM users;` | 전체 필드 반환 | PASS | `select_all` |
| 130 | Projection | projection + WHERE id | 함께 동작 | PASS | `db_select_projected()` |
| 131 | Projection | projection + WHERE name | 함께 동작 | PASS | 동일 |
| 132 | JSON | insert 결과 rows 배열 | 1행 배열 | PASS | 구현 존재 |
| 133 | JSON | select 결과 rows 배열 | 배열 형태 | PASS | 동일 |
| 134 | JSON | projection 결과 age 제거 | age 미포함 | PASS | 테스트 포함 |
| 135 | JSON | field 순서 보존 | SQL 순서와 일치 | PASS | columns 배열 순회 |
| 136 | JSON | 빈 결과 projection | 빈 배열 | PASS | 구조상 가능 |
| 137 | JSON | 모든 필드 false + 컬럼 없음 | 전체 컬럼 fallback | PASS | `all_columns_projection()` |
| 138 | JSON | builder 메모리 실패 | 에러 처리 | PARTIAL | 경로 존재, 실행 미검증 |
| 139 | UX | JSON pretty print | 보기 쉬움 | FAIL | 한 줄 응답 |
| 140 | UX | CLI 출력 친화성 | 사람 읽기 좋음 | PARTIAL | API JSON 중심 |

### H. B+ 트리와 인덱스 사용 (141-160)

| ID | 영역 | 테스트 케이스 | 기대 결과 | 판정 | 메모 |
| --- | --- | --- | --- | --- | --- |
| 141 | B+Tree | 트리 초기화 | 빈 트리 | PASS | 구현 존재 |
| 142 | B+Tree | 5000건 연속 insert | 성공 | PASS | 테스트 포함 |
| 143 | B+Tree | 첫 key 검색 | 성공 | PASS | 테스트 포함 |
| 144 | B+Tree | 중간 key 검색 | 성공 | PASS | 테스트 포함 |
| 145 | B+Tree | 마지막 key 검색 | 성공 | PASS | 테스트 포함 |
| 146 | B+Tree | 없는 key 검색 | false | PASS | 테스트 포함 |
| 147 | B+Tree | duplicate key insert | value 갱신 | PASS | 테스트 포함 |
| 148 | B+Tree | tree size 유지 | 중복 insert 시 유지 | PASS | 테스트 포함 |
| 149 | 인덱스 | `WHERE id=?` 시 index_used=true | 맞음 | PASS | `db_select_projected()` |
| 150 | 인덱스 | `WHERE name=?` 시 index_used=false | 맞음 | PASS | 동일 |
| 151 | 인덱스 | `SELECT *`는 scan 경로 | 맞음 | PASS | 동일 |
| 152 | 인덱스 | record index value 저장 | 맞음 | PASS | 설계 명확 |
| 153 | 인덱스 | startup 시 index rebuild | 성공 | PASS | `load_record()` |
| 154 | 인덱스 | explicit id insert도 index 등록 | 성공 | PASS | 내부 insert 공용 |
| 155 | 인덱스 | auto id insert도 index 등록 | 성공 | PASS | 동일 |
| 156 | 인덱스 | range query | 지원 | FAIL | 미구현 |
| 157 | 인덱스 | delete | 지원 | FAIL | 미구현 |
| 158 | 인덱스 | update 후 index 유지 | 지원 | FAIL | UPDATE 없음 |
| 159 | 성능 | indexed lookup vs linear scan 비교 장치 | 있음 | PASS | `index_used`, benchmark |
| 160 | 성능 | 인덱스 구조 설명 가능성 | 높음 | PASS | 학습용으론 좋음 |

### I. 성능, 대용량, 7주차 충족도 (161-180)

| ID | 영역 | 테스트 케이스 | 기대 결과 | 판정 | 메모 |
| --- | --- | --- | --- | --- | --- |
| 161 | 성능 | benchmark 스크립트 존재 | 있음 | PASS | `scripts/benchmark.sh` |
| 162 | 성능 | concurrent demo 스크립트 존재 | 있음 | PASS | `scripts/concurrency_demo.sh` |
| 163 | 성능 | 기본 benchmark COUNT=1000 | 실행 가능 | PASS | 기본값 1000 |
| 164 | 성능 | 1만 건 테스트 시도 가능 | 가능 | PARTIAL | 스크립트상 가능, 검증 없음 |
| 165 | 성능 | 10만 건 테스트 시도 가능 | 가능 | PARTIAL | 동일 |
| 166 | 성능 | 100만 건 테스트 기본 제공 | 충분 | FAIL | 기본 시나리오 아님 |
| 167 | 성능 | 100만 건 이상 결과 문서 제공 | 있음 | FAIL | 없음 |
| 168 | 성능 | ID lookup과 name lookup 수치 비교표 | 있음 | FAIL | 자동 리포트 없음 |
| 169 | 성능 | 평균/최솟값/최댓값 측정 | 있음 | FAIL | 없음 |
| 170 | 성능 | 순수 DB 엔진 벤치 | 있음 | FAIL | HTTP 기반만 보임 |
| 171 | 성능 | warm-up 고려 | 있음 | FAIL | 없음 |
| 172 | 성능 | 대량 insert 데이터 생성 자동화 | 부분 제공 | PARTIAL | shell loop 수준 |
| 173 | 성능 | 7주차 “1,000,000개 이상” 명시 충족 | 예 | FAIL | 입증 부족 |
| 174 | 성능 | 7주차 “ID 기준 vs 다른 필드 기준 비교” | 예 | PARTIAL | 의도는 있으나 결과 약함 |
| 175 | 성능 | 메모리 사용량 관찰 | 있음 | FAIL | 없음 |
| 176 | 성능 | 파일 크기 증가 관찰 | 있음 | FAIL | 없음 |
| 177 | 성능 | 대용량 시 index rebuild 시간 측정 | 있음 | FAIL | 없음 |
| 178 | 성능 | 발표용 그래프 생성 | 있음 | FAIL | 없음 |
| 179 | 요구 | 7주차 핵심 개념 반영 | 예 | PASS | index 활용은 분명 |
| 180 | 종합 | 7주차 완성도 | 높음 | PARTIAL | 구현은 좋지만 검증 부족 |

### J. 동시성, 안정성, 테스트 문화, 제출 완성도 (181-200)

| ID | 영역 | 테스트 케이스 | 기대 결과 | 판정 | 메모 |
| --- | --- | --- | --- | --- | --- |
| 181 | 동시성 | fixed-size thread pool | 동작 | PASS | 구현 존재 |
| 182 | 동시성 | bounded fd queue | 동작 | PASS | 구현 존재 |
| 183 | 동시성 | empty queue wait | 동작 | PASS | condvar 사용 |
| 184 | 동시성 | full queue wait | 동작 | PASS | condvar 사용 |
| 185 | 동시성 | worker shutdown wakeup | 동작 | PASS | broadcast |
| 186 | 동시성 | SELECT read lock | 병렬 허용 | PASS | rwlock |
| 187 | 동시성 | INSERT write lock | 배타 수행 | PASS | rwlock |
| 188 | 동시성 | coarse global DB lock | 안전성 높음 | PASS | 단순하고 안전 |
| 189 | 동시성 | fine-grained lock 부재 | 확장성 좋음 | FAIL | 병목 가능 |
| 190 | 동시성 | concurrent integration test 존재 | 있음 | FAIL | 없음 |
| 191 | 안정성 | append 후 index 실패 rollback | 있음 | FAIL | rollback 없음 |
| 192 | 안정성 | crash recovery | 있음 | FAIL | WAL/redo 없음 |
| 193 | 안정성 | malformed HTTP 통합 테스트 | 있음 | FAIL | 없음 |
| 194 | 테스트 | parser 단위 테스트 | 있음 | PASS | `tests/test_main.c` |
| 195 | 테스트 | B+Tree 단위 테스트 | 있음 | PASS | 동일 |
| 196 | 테스트 | DB reload 테스트 | 있음 | PASS | 동일 |
| 197 | 테스트 | raw/JSON body 테스트 | 있음 | PASS | 동일 |
| 198 | 테스트 | server end-to-end 테스트 | 있음 | FAIL | 없음 |
| 199 | 요구 | 8주차 API 서버 핵심 구조 충족 | 예 | PASS | accept, thread pool, DB 연동 |
| 200 | 최종 | 현재 브랜치가 요구사항을 제대로 구현했는가 | 일부는 예, 전체는 아니오 | PARTIAL | 8주차는 강하고 6주차, 7주차는 부족 |

## 구현 및 UX적으로 부족한 점

### 1. 6주차 요구와 실제 진입점이 어긋난다

이번 결과물의 핵심 진입점은 `HTTP 서버`다.  
그러나 6주차 요구사항의 핵심 문장은 “텍스트 파일 SQL을 Command Line으로 SQL 처리기에 전달”이다.  
즉, parser와 file DB는 있어도 **학생 입장에서 6주차 산출물의 얼굴이 되는 CLI 처리기**가 없다.

### 2. 7주차 성능 검증이 설득력 있게 닫히지 않았다

인덱스 자체는 잘 들어갔다.  
문제는 “빠르다”를 보여 주는 증거가 약하다는 점이다.

- 100만 건 이상 삽입 결과가 문서화돼 있지 않다.
- HTTP 오버헤드와 엔진 성능이 분리되지 않았다.
- 평균, 분산, 반복 측정, 그래프 같은 발표용 자료가 없다.

그래서 구현은 있지만, **7주차 과제를 평가하는 입장에서는 검증이 부족하다.**

### 3. 8주차 서버는 구조는 좋지만 견고성은 낮다

thread pool, `pthread_rwlock_t`, API routing, JSON 응답 구조는 학습용으로 좋다.  
반면 실제 서버답게 보이려면 아래가 부족하다.

- robust JSON parser
- Content-Type 검증
- chunked/keep-alive 대응
- 파일 쓰기 원자성
- rollback 또는 복구 전략

### 4. 문서 UX가 아쉽다

현재 브랜치의 `README.md`, `requirements.md`는 이번 환경에서 인코딩이 깨져 보였다.  
이 문제는 단순 미관 이슈가 아니다. 수업, 발표, 코드리뷰 상황에서 **문서를 읽는 경험 자체를 망가뜨린다.**

### 5. 과제 확장성보다 하드코딩이 강하다

`users` 단일 테이블, 제한된 SQL, CSV 구조 보호를 위한 name 제한 등은 학습용으로는 괜찮다.  
하지만 “DBMS처럼 보이는 결과물”을 기대하면 확장성이 약하다.

## 최종 결론

현재 브랜치의 프로젝트는 **학습용 미니 DBMS API 서버로는 꽤 잘 정리된 편**이다.  
특히 parser, DB engine, B+Tree, thread pool, API 서버가 연결된 흐름은 학생이 설명하기 좋은 구조다.

하지만 요구사항을 그대로 기준 삼으면 최종 평가는 아래가 가장 공정하다.

> **8주차는 대체로 충족, 6주차와 7주차는 부분 충족, 전체적으로는 부분 충족**

즉, “아예 못 만들었다”는 평가는 부당하지만,  
“요구사항을 전부 제대로 구현했다”고 말하기에도 부족하다.

