# 03-04. SQL Parser: 문자열을 실행 의도로 바꾸기

## 먼저 한 문장으로 보기
`sql.c`는 SQL 문자열을 직접 실행하지 않고, DB engine이 이해할 수 있는 `SqlStatement` 구조체로 변환합니다.

## 요청 흐름에서의 위치
```text
HTTP body에서 SQL 추출
  -> sql_parse()
  -> SqlStatement
  -> execute_statement()
  -> db_insert() 또는 db_select()
```

## 이 코드를 읽기 전에 알아야 할 CS 개념
| 개념 | 짧은 설명 | 이 파일에서 보이는 지점 |
| --- | --- | --- |
| parsing | 문자열을 의미 있는 구조로 바꾸는 작업입니다. | `sql_parse()` |
| grammar | 허용되는 문장의 규칙입니다. | 지원 SQL 네 가지 |
| token-like reading | keyword, identifier, literal을 차례로 읽습니다. | `read_word()`, `parse_string_literal()` |
| validation | 지원하지 않는 문법이나 잘못된 값을 거부합니다. | error buffer |
| structured intent | 실행기가 이해하기 쉬운 형태로 의도를 표현합니다. | `SqlStatement` |

## SQL 실행과 SQL 파싱은 다르다
SQL 파싱은 “문자열의 의미를 구조화하는 일”입니다. SQL 실행은 “그 의미대로 데이터를 읽거나 쓰는 일”입니다.

예를 들어 다음 SQL이 있습니다.

```sql
SELECT * FROM users WHERE id = 1;
```

parser는 이 SQL을 실행하지 않습니다. 대신 이런 정보를 뽑습니다.

```text
type: SQL_SELECT
table: users
where_type: SQL_WHERE_ID
where_id: 1
```

이 구조화 덕분에 DB engine은 문자열을 다시 해석하지 않고, 명시적인 필드를 보고 실행할 수 있습니다.

## 주요 구조체: `SqlStatement`
- 이름: `SqlStatement`
- 위치: `include/sql.h`
- 한 문장 목적: SQL 문자열에서 파악한 실행 의도를 담습니다.
- 주요 필드:
  - `type`: `SQL_INSERT` 또는 `SQL_SELECT`
  - `table`: 현재는 `users`만 지원
  - `insert_name`, `insert_age`: INSERT 값
  - `where_type`: 조건 없음, id 조건, name 조건
  - `where_id`, `where_name`: SELECT 조건 값
- 학습자가 확인할 질문:
  - SQL이 추가될수록 이 구조체는 어떻게 달라져야 할까요?

## API 카드: `sql_parse`
- 이름: `sql_parse`
- 위치: `src/sql.c`, 선언은 `include/sql.h`
- 한 문장 목적: SQL 문자열 전체를 보고 INSERT parser 또는 SELECT parser로 분기합니다.
- 입력:
  - `const char *sql`
  - `SqlStatement *stmt`
  - `char *err`
  - `size_t err_size`
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점: `handle_query()`에서 JSON body에서 SQL을 꺼낸 뒤
- 내부에서 하는 일:
  - 앞쪽 공백을 건너뜁니다.
  - 빈 SQL인지 확인합니다.
  - `INSERT`로 시작하면 `parse_insert()`를 호출합니다.
  - `SELECT`로 시작하면 `parse_select()`를 호출합니다.
  - 그 외 문장은 지원하지 않는 SQL로 처리합니다.
- 실패할 수 있는 지점:
  - 빈 SQL
  - 지원하지 않는 명령
  - INSERT/SELECT 내부 문법 오류
- 학습자가 확인할 질문:
  - 왜 이 함수가 DB 파일이나 B+ tree를 전혀 알 필요가 없을까요?

## API 카드: `parse_insert`
- 이름: `parse_insert`
- 위치: `src/sql.c`
- 한 문장 목적: 지원하는 INSERT 문법을 검사하고 INSERT용 필드를 채웁니다.
- 입력: SQL 문자열, `SqlStatement *stmt`, error buffer
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점: `sql_parse()`가 SQL이 INSERT로 시작한다고 판단한 뒤
- 지원 문법:
```sql
INSERT INTO users name age VALUES 'kim' 20;
```
- 내부에서 하는 일:
  - `INSERT INTO` keyword를 확인합니다.
  - table이 `users`인지 확인합니다.
  - column이 `name age` 순서인지 확인합니다.
  - `VALUES` keyword를 확인합니다.
  - single-quoted name을 읽습니다.
  - age 정수를 읽습니다.
  - 문장이 끝났는지 확인합니다.
- 실패할 수 있는 지점:
  - table이 `users`가 아님
  - column 순서가 다름
  - name이 작은따옴표로 감싸져 있지 않음
  - age가 음수이거나 숫자가 아님
- 학습자가 확인할 질문:
  - column 순서를 자유롭게 허용하려면 parser가 무엇을 더 해야 할까요?

## API 카드: `parse_select`
- 이름: `parse_select`
- 위치: `src/sql.c`
- 한 문장 목적: 지원하는 SELECT 문법을 검사하고 WHERE 조건을 구조화합니다.
- 입력: SQL 문자열, `SqlStatement *stmt`, error buffer
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점: `sql_parse()`가 SQL이 SELECT로 시작한다고 판단한 뒤
- 지원 문법:
```sql
SELECT * FROM users;
SELECT * FROM users WHERE id = 1;
SELECT * FROM users WHERE name = 'kim';
```
- 내부에서 하는 일:
  - `SELECT * FROM`을 확인합니다.
  - table이 `users`인지 확인합니다.
  - 문장이 끝나면 전체 조회로 처리합니다.
  - `WHERE id = 숫자`면 `SQL_WHERE_ID`를 설정합니다.
  - `WHERE name = '문자열'`이면 `SQL_WHERE_NAME`을 설정합니다.
- 실패할 수 있는 지점:
  - `*`가 없음
  - 지원하지 않는 table
  - 지원하지 않는 WHERE column
  - WHERE value 형식 오류
- 학습자가 확인할 질문:
  - `WHERE age = 20`을 추가하려면 parser와 DB engine 중 어디를 바꿔야 할까요?

## API 카드: `read_word`
- 이름: `read_word`
- 위치: `src/sql.c`
- 한 문장 목적: 현재 위치에서 identifier 또는 keyword 하나를 읽습니다.
- 입력: `const char **p`, output buffer, buffer size
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점: keyword, table 이름, column 이름을 읽을 때
- 내부에서 하는 일:
  - 공백을 건너뜁니다.
  - 첫 글자가 alphabet 또는 `_`인지 확인합니다.
  - 이후 alphabet, digit, `_`를 계속 읽습니다.
  - 읽은 위치만큼 포인터를 앞으로 이동합니다.
- 실패할 수 있는 지점:
  - 단어가 시작되지 않음
  - output buffer보다 단어가 김
- 학습자가 확인할 질문:
  - `const char **p`처럼 포인터의 포인터를 받는 이유는 무엇일까요?

## API 카드: `parse_string_literal`
- 이름: `parse_string_literal`
- 위치: `src/sql.c`
- 한 문장 목적: 작은따옴표로 감싼 문자열 값을 읽습니다.
- 입력: 현재 SQL 위치 포인터, output buffer, buffer size
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점: INSERT name, WHERE name 값을 읽을 때
- 내부에서 하는 일:
  - 첫 문자가 `'`인지 확인합니다.
  - 다음 `'`가 나올 때까지 문자를 복사합니다.
  - 닫는 `'` 이후로 포인터를 이동합니다.
- 실패할 수 있는 지점:
  - 여는 작은따옴표가 없음
  - 닫는 작은따옴표가 없음
  - 문자열이 buffer보다 김
- 학습자가 확인할 질문:
  - 문자열 안에 작은따옴표를 넣고 싶다면 어떤 escaping 규칙이 필요할까요?

## API 카드: `parse_int_value`
- 이름: `parse_int_value`
- 위치: `src/sql.c`
- 한 문장 목적: 현재 위치에서 non-negative integer를 읽습니다.
- 입력: 현재 SQL 위치 포인터, `int *out`
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점: age, id 값을 읽을 때
- 내부에서 하는 일:
  - `strtol()`로 숫자를 파싱합니다.
  - 음수와 int 범위 초과를 거부합니다.
  - 읽은 위치만큼 포인터를 이동합니다.
- 실패할 수 있는 지점:
  - 숫자가 없음
  - 음수
  - int 범위 초과
- 학습자가 확인할 질문:
  - `atoi()` 대신 `strtol()`을 쓰면 어떤 점이 더 안전할까요?

## 최소 문법의 의미
이 parser는 SQL 표준 전체를 구현하지 않습니다. 하지만 그 덕분에 “문자열을 구조체로 바꾸는 기본 사고”가 잘 보입니다.

실제 SQL parser는 tokenizing, grammar, AST, query planner로 발전합니다. 이 프로젝트의 `SqlStatement`는 그중 AST와 execution plan 사이의 아주 작은 형태로 볼 수 있습니다.

## 코드 관찰 포인트
- `sql_parse()`는 SQL의 첫 keyword만 보고 INSERT와 SELECT parser로 분기합니다.
- helper 함수들은 포인터를 앞으로 이동시키며 입력을 소비합니다. 그래서 `const char **p`가 자주 등장합니다.
- parser는 실행하지 않습니다. 파일을 열지도 않고 B+ tree를 찾지도 않습니다.
- 에러 메시지는 사용자에게 반환될 수 있으므로, 실패 이유를 비교적 구체적으로 남깁니다.

## 흔한 오해
| 오해 | 바로잡기 |
| --- | --- |
| parser는 SQL을 실행한다. | parser는 의미를 구조화할 뿐이고 실행은 DB engine이 합니다. |
| 문법을 작게 제한하면 parser가 의미 없다. | 작은 문법도 parsing과 execution boundary를 배우기에 충분합니다. |
| `WHERE id`와 `WHERE name`은 DB에서만 구분된다. | parser가 먼저 서로 다른 `where_type`으로 구분해 줍니다. |

## 다음 문서로 넘어가기
이제 구조화된 SQL 의도가 실제 파일 저장과 조회로 실행되는 과정을 봅니다.

다음: [05_db_engine.md](05_db_engine.md)
