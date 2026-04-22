# 6주차~8주차 요구사항 평가서 및 200개 테스트 케이스 결과

## 이 문서의 목적
이 문서는 현재 프로젝트를 6주차 SQL 처리기, 7주차 B+ 트리 인덱스, 8주차 미니 DBMS API 서버 요구사항에 맞춰 평가한 결과 보고서다. 관점은 "좋아 보이는가"보다 먼저 "요구사항을 얼마나 정확히 만족하는가"에 둔다.

평가 대상의 핵심 구현 파일은 다음과 같다.

- [`src/main.c`](../src/main.c:18)
- [`src/sql.c`](../src/sql.c:302)
- [`src/db.c`](../src/db.c:129)
- [`src/bptree.c`](../src/bptree.c:227)
- [`src/http.c`](../src/http.c:238)
- [`src/thread_pool.c`](../src/thread_pool.c:29)
- [`src/server.c`](../src/server.c:257)
- [`tests/test_main.c`](../tests/test_main.c:1)

## 검증 방법과 한계
이번 평가는 두 층으로 진행했다.

1. 코드 정적 검증
   - 요구사항과 실제 구현 파일을 대조했다.
   - 구현 여부를 `PASS`, `PARTIAL`, `FAIL`로 판정했다.
2. 실행 가능성 점검
   - 현재 세션에는 `cc`, `gcc`, `clang`, `make`가 없어 실제 바이너리 빌드와 런타임 검증이 불가능했다.
   - 따라서 실행 기반으로만 확인 가능한 항목은 `BLOCKED`로 표시했다.

즉 이 보고서는 "실행해서 모두 통과했다"가 아니라, "현재 환경에서 확인 가능한 최대한의 근거를 바탕으로 판단한 결과"다.

## 핵심 총평
이 결과물은 8주차 API 서버 요구사항 기준으로 보면 꽤 잘 구현된 편이다. 외부 클라이언트가 HTTP로 SQL을 보내고, 서버가 thread pool을 통해 병렬로 처리하며, 내부에서는 SQL parser와 B+ 트리 인덱스가 연결된다.

다만 6주차 요구사항을 엄밀히 적용하면, "텍스트파일로 작성된 SQL문을 CLI 명령으로 SQL 처리기에 전달"하는 형태는 구현되어 있지 않다. 현재 결과물은 SQL CLI 처리기라기보다 API 서버 중심 구조다. 7주차도 B+ 트리 연동은 되어 있지만, 1,000,000건 이상의 대규모 성능 테스트 자동화는 부족하다.

한 줄로 정리하면:

> 코드 품질과 구조는 괜찮지만, 6주차~8주차 원래 과제를 모두 1:1로 충족한 결과물이라고 보기는 어렵다.

## 주차별 총평

### 6주차 평가
- 강점: SQL 파싱, INSERT/SELECT 실행, 파일 기반 저장 개념은 들어 있다.
- 약점: CLI SQL processor 요구사항이 빠졌다.
- 판정: `부분 충족`

### 7주차 평가
- 강점: 메모리 기반 B+ 트리 구현, ID 인덱싱, 인덱스 기반 조회와 선형 탐색 비교 구조 존재.
- 약점: 1,000,000건 이상 자동 검증과 성능 기록 체계 부족.
- 판정: `대체로 충족`

### 8주차 평가
- 강점: API 서버, thread pool, DB 엔진 연결, 동시성 제어 구조가 명확하다.
- 약점: 통합 테스트와 동시성 회귀 테스트가 약하다.
- 판정: `대체로 충족`

## 요구사항별 핵심 판정

### 6주차 SQL 처리기
- `PASS`: C 언어로 구현
- `PASS`: INSERT, SELECT 파싱
- `PASS`: INSERT 시 파일 append
- `PARTIAL`: SELECT는 "파일에서 직접 읽기"보다 서버 시작 시 메모리 적재 후 메모리 조회
- `FAIL`: SQL 텍스트 파일을 CLI 인자로 넘겨 실행하는 구조 없음
- `FAIL`: SQL 처리 결과를 CLI 프로그램으로 출력하는 구조 없음

### 7주차 B+ 트리 인덱스
- `PASS`: 메모리 기반 B+ 트리 구현
- `PASS`: ID 자동 부여 및 인덱스 등록
- `PASS`: `WHERE id = ?` 인덱스 사용
- `PASS`: 다른 필드 기준 선형 탐색
- `PARTIAL`: 성능 비교용 benchmark 스크립트 존재
- `FAIL`: 1,000,000건 이상 자동 실험 근거 부족

### 8주차 API 서버
- `PASS`: 외부 클라이언트가 HTTP API로 DB 기능 사용 가능
- `PASS`: thread pool 기반 병렬 처리 구조 존재
- `PASS`: SQL 처리기와 B+ 트리 인덱스를 내부 DB 엔진으로 연동
- `PASS`: DB 전역 read-write lock으로 최소한의 동시성 제어 수행
- `PARTIAL`: 동시성 검증과 서버 통합 검증이 약함

## 구현과 UX 관점에서 드러난 부족한 점
- README와 실제 코드가 완전히 일치하지 않는다. 특히 문서에는 JSON body와 raw SQL body, SQL 문법 예시가 혼재되어 있다.
- 6주차 핵심이던 CLI 경험이 빠져 있다. 지금은 `curl`이나 API 호출이 사실상 유일한 사용 경로다.
- HTTP 파서와 JSON 파서는 최소 구현이다. 교육용으로는 괜찮지만 견고한 서버 관점에서는 약하다.
- 동시성 제어는 단순한 전역 `pthread_rwlock_t` 한 개라 이해는 쉽지만 확장성은 제한적이다.
- B+ 트리와 DB는 연결되어 있지만, 대규모 성능 실험과 결과 기록은 부족하다.
- 테스트는 함수 단위가 중심이다. 실제 네트워크 통합 테스트, 오류 주입 테스트, 다중 스레드 회귀 테스트는 약하다.
- 현재 환경에서는 바로 빌드가 되지 않는다. POSIX 의존 프로젝트인데 컴파일 환경 안내와 재현 경로가 약하다.

## 200개 테스트 케이스와 결과

### 6주차 관련 테스트 1~70
1. `[W6-001]` 구현 언어가 C인가? 결과: `src/*.c`, `include/*.h` 확인. 판정: `PASS`
2. `[W6-002]` 실행 진입점 `main()`이 있는가? 결과: [`src/main.c`](../src/main.c:18). 판정: `PASS`
3. `[W6-003]` CLI 인자로 포트/스레드/파일 경로를 받는가? 결과: `argv[1..3]` 처리. 판정: `PASS`
4. `[W6-004]` CLI 인자로 SQL 텍스트 파일 경로를 받는가? 결과: 없음. 판정: `FAIL`
5. `[W6-005]` SQL 텍스트 파일을 읽는 처리기 함수가 있는가? 결과: 없음. 판정: `FAIL`
6. `[W6-006]` SQL를 stdin/CLI로 받아 실행하는가? 결과: 없음. 판정: `FAIL`
7. `[W6-007]` INSERT 문 파싱 지원 여부. 결과: [`src/sql.c`](../src/sql.c:202). 판정: `PASS`
8. `[W6-008]` SELECT 문 파싱 지원 여부. 결과: [`src/sql.c`](../src/sql.c:252). 판정: `PASS`
9. `[W6-009]` 최소 지원 문법 INSERT/SELECT 충족 여부. 결과: 충족. 판정: `PASS`
10. `[W6-010]` DELETE를 거절하는가? 결과: parser가 INSERT/SELECT 외 거절. 판정: `PASS`
11. `[W6-011]` UPDATE를 거절하는가? 결과: parser가 거절. 판정: `PASS`
12. `[W6-012]` CREATE TABLE을 거절하는가? 결과: parser가 거절. 판정: `PASS`
13. `[W6-013]` 키워드 대소문자 혼합 허용 여부. 결과: `strcasecmp/strncasecmp` 사용. 판정: `PASS`
14. `[W6-014]` `INSERT INTO users VALUES (id,'name',age)` 구문 지원 여부. 결과: [`src/sql.c`](../src/sql.c:163). 판정: `PASS`
15. `[W6-015]` `INSERT INTO users name age VALUES 'kim' 20` 지원 여부. 결과: [`src/sql.c`](../src/sql.c:223). 판정: `PASS`
16. `[W6-016]` `INSERT INTO users VALUES (...)`에서 양의 id 요구 여부. 결과: `expected positive id`. 판정: `PASS`
17. `[W6-017]` `INSERT` 시 음수 age 거절 여부. 결과: `parse_int_value`와 DB 검증. 판정: `PASS`
18. `[W6-018]` INSERT 이름이 작은따옴표 문자열이어야 하는가? 결과: `parse_string_literal`. 판정: `PASS`
19. `[W6-019]` `SELECT * FROM users` 지원 여부. 결과: `parse_select_columns`. 판정: `PASS`
20. `[W6-020]` `SELECT id, name FROM users` projection 지원 여부. 결과: 구현됨. 판정: `PASS`
21. `[W6-021]` `SELECT name, id FROM users` 컬럼 순서 유지 여부. 결과: 테스트 존재. 판정: `PASS`
22. `[W6-022]` `SELECT age FROM users` 단일 projection 가능 여부. 결과: parser/DB 구조상 가능. 판정: `PASS`
23. `[W6-023]` `SELECT id, id FROM users` 중복 컬럼 거절 여부. 결과: `duplicate SELECT column`. 판정: `PASS`
24. `[W6-024]` `SELECT foo FROM users` 거절 여부. 결과: column parser가 거절. 판정: `PASS`
25. `[W6-025]` `WHERE id = 42` 지원 여부. 결과: 지원. 판정: `PASS`
26. `[W6-026]` `WHERE name = 'lee'` 지원 여부. 결과: 지원. 판정: `PASS`
27. `[W6-027]` `WHERE age = 20` 거절 여부. 결과: `only WHERE id or WHERE name is supported`. 판정: `PASS`
28. `[W6-028]` `WHERE id > 1` 거절 여부. 결과: `=`만 허용. 판정: `PASS`
29. `[W6-029]` 테이블명이 `users` 외이면 거절되는가? 결과: 고정 테이블만 허용. 판정: `PASS`
30. `[W6-030]` 빈 SQL 거절 여부. 결과: `empty SQL`. 판정: `PASS`
31. `[W6-031]` 공백만 있는 SQL 거절 여부. 결과: `skip_ws` 후 거절. 판정: `PASS`
32. `[W6-032]` 세미콜론이 없어도 끝으로 인정하는가? 결과: `at_statement_end`에서 허용. 판정: `PASS`
33. `[W6-033]` SQL 뒤 쓰레기 토큰이 있으면 거절되는가? 결과: `unexpected trailing tokens`. 판정: `PASS`
34. `[W6-034]` 문자열 escape SQL 문법 지원 여부. 결과: SQL string escape는 없음. 판정: `FAIL`
35. `[W6-035]` schema/table 이미 존재 가정 충족 여부. 결과: `users` 고정, CREATE TABLE 없음. 판정: `PASS`
36. `[W6-036]` INSERT가 파일에 append 되는가? 결과: [`src/db.c`](../src/db.c:223) `fopen(...,"a")`. 판정: `PASS`
37. `[W6-037]` SELECT가 실행 결과를 반환하는가? 결과: `DbResult.rows_json` 반환. 판정: `PASS`
38. `[W6-038]` 각 테이블이 파일로 관리되는가? 결과: 현재는 사실상 `users` 파일 하나. 판정: `PARTIAL`
39. `[W6-039]` 저장 포맷 자유 설계 충족 여부. 결과: CSV-like. 판정: `PASS`
40. `[W6-040]` 서버 시작 시 파일을 읽어 메모리 적재하는가? 결과: [`src/db.c`](../src/db.c:145). 판정: `PASS`
41. `[W6-041]` SELECT가 매번 파일을 읽는 구조인가? 결과: 아님, 메모리 조회. 판정: `PARTIAL`
42. `[W6-042]` 파일이 없을 때 새로 생성 가능한가? 결과: `a+` 사용. 판정: `PASS`
43. `[W6-043]` malformed data file 거절 여부. 결과: `sscanf != 3`면 실패. 판정: `PASS`
44. `[W6-044]` 파일 내 invalid id/age/name 감지 여부. 결과: validation 존재. 판정: `PASS`
45. `[W6-045]` auto-increment id 부여 여부. 결과: `next_id` 사용. 판정: `PASS`
46. `[W6-046]` 명시 id 삽입 지원 여부. 결과: `db_insert_with_id`. 판정: `PASS`
47. `[W6-047]` duplicate id 삽입 거절 여부. 결과: `duplicate id`. 판정: `PASS`
48. `[W6-048]` name empty 거절 여부. 결과: `name_is_valid`. 판정: `PASS`
49. `[W6-049]` name에 comma 포함 거절 여부. 결과: `name_is_valid`. 판정: `PASS`
50. `[W6-050]` name에 newline 포함 거절 여부. 결과: `name_is_valid`. 판정: `PASS`
51. `[W6-051]` DB 결과 메모리 해제 함수 존재 여부. 결과: `db_result_free`. 판정: `PASS`
52. `[W6-052]` parser unit test 존재 여부. 결과: [`tests/test_main.c`](../tests/test_main.c:9). 판정: `PASS`
53. `[W6-053]` DB insert/select/reload test 존재 여부. 결과: [`tests/test_main.c`](../tests/test_main.c:72). 판정: `PASS`
54. `[W6-054]` raw SQL body extraction test 존재 여부. 결과: [`tests/test_main.c`](../tests/test_main.c:143). 판정: `PASS`
55. `[W6-055]` JSON body SQL extraction test 존재 여부. 결과: [`tests/test_main.c`](../tests/test_main.c:149). 판정: `PASS`
56. `[W6-056]` 결과를 CLI stdout에 human-readable하게 출력하는가? 결과: 별도 SQL CLI 없음. 판정: `FAIL`
57. `[W6-057]` 한 파일에 SQL 여러 개를 연속 실행하는 처리기 존재 여부. 결과: 없음. 판정: `FAIL`
58. `[W6-058]` SQL 처리기만 따로 바이너리로 빌드되는가? 결과: 없음, 서버 바이너리만 존재. 판정: `FAIL`
59. `[W6-059]` README가 SQL CLI 사용법을 안내하는가? 결과: API 사용법만 안내. 판정: `FAIL`
60. `[W6-060]` SQL parser 에러 메시지가 비교적 구체적인가? 결과: 구체적. 판정: `PASS`
61. `[W6-061]` SELECT projection 결과에서 불필요한 필드가 빠지는가? 결과: `db_select_projected`. 판정: `PASS`
62. `[W6-062]` projection 순서를 유지하는가? 결과: 테스트 존재. 판정: `PASS`
63. `[W6-063]` `SELECT *`와 projection 로직이 공존하는가? 결과: 지원. 판정: `PASS`
64. `[W6-064]` SQL parser와 실행 로직이 분리되어 있는가? 결과: `sql.c`와 `db.c` 분리. 판정: `PASS`
65. `[W6-065]` DB 파일 경로를 CLI로 바꿀 수 있는가? 결과: 가능. 판정: `PASS`
66. `[W6-066]` 현재 구현이 6주차 핵심인 "입력->파싱->실행->저장" 흐름은 갖췄는가? 결과: 서버 내부적으로는 갖춤. 판정: `PASS`
67. `[W6-067]` 현재 구현이 6주차 핵심인 "CLI 처리기"를 갖췄는가? 결과: 없음. 판정: `FAIL`
68. `[W6-068]` 현 환경에서 `make test`로 6주차 기능 실행 검증 가능한가? 결과: 컴파일러 부재. 판정: `BLOCKED`
69. `[W6-069]` 6주차 결과물로 제출 시 요구사항 해석 논란이 생길 소지가 있는가? 결과: CLI 부재로 큼. 판정: `PARTIAL`
70. `[W6-070]` 6주차 총평. 결과: SQL 처리 로직은 좋지만 CLI 과제 산출물로는 부족. 판정: `PARTIAL`

### 7주차 관련 테스트 71~135
71. `[W7-071]` B+ 트리 공개 API 존재 여부. 결과: [`include/bptree.h`](../include/bptree.h:1). 판정: `PASS`
72. `[W7-072]` 메모리 기반 B+ 트리인가? 결과: 동적 노드 구조, 디스크 페이지 없음. 판정: `PASS`
73. `[W7-073]` `bptree_init` 구현 여부. 결과: 존재. 판정: `PASS`
74. `[W7-074]` `bptree_destroy` 구현 여부. 결과: 존재. 판정: `PASS`
75. `[W7-075]` `bptree_insert` 구현 여부. 결과: 존재. 판정: `PASS`
76. `[W7-076]` `bptree_search` 구현 여부. 결과: 존재. 판정: `PASS`
77. `[W7-077]` insert 시 새 루트 생성 가능 여부. 결과: 구현됨. 판정: `PASS`
78. `[W7-078]` leaf split 로직 존재 여부. 결과: [`src/bptree.c`](../src/bptree.c:63). 판정: `PASS`
79. `[W7-079]` internal split 로직 존재 여부. 결과: [`src/bptree.c`](../src/bptree.c:142). 판정: `PASS`
80. `[W7-080]` duplicate key update 시 size 유지 여부. 결과: 테스트 존재. 판정: `PASS`
81. `[W7-081]` search가 internal node를 따라 내려가는가? 결과: `child_index_for_key`. 판정: `PASS`
82. `[W7-082]` leaf `next` 포인터 유지 여부. 결과: split 시 연결 유지. 판정: `PASS`
83. `[W7-083]` DB 초기화 시 파일의 기존 레코드를 인덱스에 넣는가? 결과: `load_record -> bptree_insert`. 판정: `PASS`
84. `[W7-084]` INSERT 시 새 ID를 인덱스에 등록하는가? 결과: `db_insert_internal`. 판정: `PASS`
85. `[W7-085]` `WHERE id = ?`가 B+ 트리를 사용하는가? 결과: `db_select_projected` 분기. 판정: `PASS`
86. `[W7-086]` `WHERE name = ?`는 선형 탐색인가? 결과: records 순회. 판정: `PASS`
87. `[W7-087]` 인덱스 사용 여부를 결과에 담는가? 결과: `index_used`. 판정: `PASS`
88. `[W7-088]` 인덱스 미사용 검색도 성능 비교 가능하게 메타데이터가 있는가? 결과: `elapsed_us`. 판정: `PASS`
89. `[W7-089]` 자동 ID 생성과 명시 ID 삽입이 함께 가능한가? 결과: 둘 다 지원. 판정: `PASS`
90. `[W7-090]` duplicate id를 인덱스 레벨에서 걸러내는가? 결과: DB 레벨에서 search 후 거절. 판정: `PASS`
91. `[W7-091]` 없는 id search 시 false를 반환하는가? 결과: 구현상 그렇다. 판정: `PASS`
92. `[W7-092]` B+ 트리 unit test 존재 여부. 결과: [`tests/test_main.c`](../tests/test_main.c:51). 판정: `PASS`
93. `[W7-093]` 5000건 삽입 테스트 존재 여부. 결과: 존재. 판정: `PASS`
94. `[W7-094]` 1,000,000건 이상 삽입 자동 테스트 존재 여부. 결과: 없음. 판정: `FAIL`
95. `[W7-095]` benchmark 스크립트 존재 여부. 결과: [`scripts/benchmark.sh`](../scripts/benchmark.sh:1). 판정: `PASS`
96. `[W7-096]` benchmark 기본 COUNT가 1,000,000 이상인가? 결과: 1000. 판정: `FAIL`
97. `[W7-097]` benchmark가 indexed lookup과 linear lookup을 비교하는가? 결과: 비교함. 판정: `PASS`
98. `[W7-098]` benchmark가 health concurrency도 함께 보는가? 결과: 추가로 수행함. 판정: `PARTIAL`
99. `[W7-099]` benchmark가 결과를 파일로 저장하는가? 결과: 없음. 판정: `FAIL`
100. `[W7-100]` benchmark가 재현 가능한 대용량 성능 실험 체계를 제공하는가? 결과: 약함. 판정: `PARTIAL`
101. `[W7-101]` `WHERE id`와 다른 필드 기준 속도 차이를 설명할 근거가 있는가? 결과: 스크립트와 응답 메타데이터 존재. 판정: `PASS`
102. `[W7-102]` B+ 트리 key/value 정의가 명확한가? 결과: `id -> record index`. 판정: `PASS`
103. `[W7-103]` 범위 검색 API가 있는가? 결과: 없음. 판정: `PARTIAL`
104. `[W7-104]` B+ 트리 노드 레벨 동시성 제어가 있는가? 결과: 없음. 판정: `PARTIAL`
105. `[W7-105]` DB 전역 lock 아래에서 인덱스를 사용하는가? 결과: 그렇다. 판정: `PASS`
106. `[W7-106]` records 배열 capacity 확장 로직 존재 여부. 결과: `db_reserve_records`. 판정: `PASS`
107. `[W7-107]` 대량 삽입 시 next_id가 정상 증가하는가? 결과: 코드상 가능. 판정: `PASS`
108. `[W7-108]` 명시 ID 삽입 후 next_id 업데이트 처리 여부. 결과: `record.id >= db->next_id`. 판정: `PASS`
109. `[W7-109]` projection과 인덱스 검색이 함께 동작하는가? 결과: `db_select_projected`에서 가능. 판정: `PASS`
110. `[W7-110]` index hit 시 단일 row JSON 직렬화가 가능한가? 결과: 가능. 판정: `PASS`
111. `[W7-111]` 선형 탐색과 projection 조합이 가능한가? 결과: 가능. 판정: `PASS`
112. `[W7-112]` restart 후 인덱스 재구성이 가능한가? 결과: test 존재. 판정: `PASS`
113. `[W7-113]` 기존 SQL 처리기를 그대로 활용해 연동했는가? 결과: parser 재사용됨. 판정: `PASS`
114. `[W7-114]` 6주차 CLI 처리기와 7주차 인덱스가 직접 합쳐진 형태인가? 결과: 아니고 서버 중심 구조로 흡수됨. 판정: `PARTIAL`
115. `[W7-115]` 삽입 데이터 생성기가 있는가? 결과: benchmark 루프 수준. 판정: `PARTIAL`
116. `[W7-116]` benchmark가 timestamp 기반 base id로 충돌을 줄이는가? 결과: 그렇다. 판정: `PASS`
117. `[W7-117]` B+ 트리 최대 키 수 고정 상수 사용 여부. 결과: `BPTREE_MAX_KEYS 31`. 판정: `PASS`
118. `[W7-118]` internal promoted key 계산 존재 여부. 결과: 존재. 판정: `PASS`
119. `[W7-119]` 메모리 부족 시 insert 실패 처리 여부. 결과: 리턴값으로 실패. 판정: `PASS`
120. `[W7-120]` duplicate key overwrite behavior가 테스트되는가? 결과: 테스트됨. 판정: `PASS`
121. `[W7-121]` 없는 이름 조회 시 빈 결과가 되는가? 결과: 코드상 가능. 판정: `PASS`
122. `[W7-122]` 인덱스 조회 시 `record_index < db->count` 검증 여부. 결과: 있음. 판정: `PASS`
123. `[W7-123]` 파일 기반 저장과 메모리 기반 인덱스 결합 설계가 명확한가? 결과: 명확함. 판정: `PASS`
124. `[W7-124]` 1,000,000건 성능 실험을 README가 명시적으로 안내하는가? 결과: 아니오. 판정: `FAIL`
125. `[W7-125]` 대용량 실험 결과 예시가 문서에 있는가? 결과: 없음. 판정: `FAIL`
126. `[W7-126]` B+ 트리 구현을 수업 관점에서 설명 가능한 구조인가? 결과: 비교적 단순함. 판정: `PASS`
127. `[W7-127]` delete/merge 미구현이 현재 요구 범위 밖인가? 결과: 그렇다. 판정: `PASS`
128. `[W7-128]` id 외 다른 인덱스를 추가하기 쉬운가? 결과: 현재 구조는 단일 인덱스 고정. 판정: `PARTIAL`
129. `[W7-129]` 현재 구조에서 age index를 추가하면 parser/DB 설계를 얼마나 바꿔야 하는가? 결과: 적지 않다. 판정: `PARTIAL`
130. `[W7-130]` 인덱스 성능 관찰용 `elapsed_us`가 DB 내부에서 계산되는가? 결과: `now_us`. 판정: `PASS`
131. `[W7-131]` 현재 환경에서 benchmark 실제 실행 검증 가능한가? 결과: 컴파일러 부재. 판정: `BLOCKED`
132. `[W7-132]` 현재 환경에서 B+ 트리 unit test 실행 가능한가? 결과: 컴파일러 부재. 판정: `BLOCKED`
133. `[W7-133]` 7주차 핵심인 "WHERE id" 인덱스 활용은 달성했는가? 결과: 달성. 판정: `PASS`
134. `[W7-134]` 7주차 핵심인 대규모 성능 시험은 달성했는가? 결과: 미흡. 판정: `FAIL`
135. `[W7-135]` 7주차 총평. 결과: 인덱스 구현은 좋지만 대용량 실험이 약하다. 판정: `PARTIAL`

### 8주차 관련 테스트 136~200
136. `[W8-136]` API 서버 엔트리포인트 존재 여부. 결과: `server_run`. 판정: `PASS`
137. `[W8-137]` 외부 클라이언트가 HTTP API로 접근 가능한가? 결과: `INADDR_ANY` 바인딩. 판정: `PASS`
138. `[W8-138]` `GET /health` 구현 여부. 결과: [`src/server.c`](../src/server.c:177). 판정: `PASS`
139. `[W8-139]` `POST /query` 구현 여부. 결과: [`src/server.c`](../src/server.c:180). 판정: `PASS`
140. `[W8-140]` `/query` 잘못된 method에 405 응답 여부. 결과: 구현됨. 판정: `PASS`
141. `[W8-141]` `/health` 잘못된 method에 405 응답 여부. 결과: 구현됨. 판정: `PASS`
142. `[W8-142]` unknown route 404 여부. 결과: 구현됨. 판정: `PASS`
143. `[W8-143]` malformed request 400 여부. 결과: `http_read_request` 실패 처리. 판정: `PASS`
144. `[W8-144]` malformed SQL 400 여부. 결과: `sql_parse` 실패 처리. 판정: `PASS`
145. `[W8-145]` DB 내부 오류 500 여부. 결과: `db_result.ok` 검사. 판정: `PASS`
146. `[W8-146]` 성공 응답이 JSON인가? 결과: `http_send_json`. 판정: `PASS`
147. `[W8-147]` 에러 응답도 JSON인가? 결과: `make_error_body`. 판정: `PASS`
148. `[W8-148]` 성공 응답에 `rows/message/index_used/elapsed_us` 포함 여부. 결과: `make_success_body`. 판정: `PASS`
149. `[W8-149]` `Connection: close` 헤더 포함 여부. 결과: 포함. 판정: `PASS`
150. `[W8-150]` HTTP keep-alive 미지원 여부. 결과: close-only. 판정: `PASS`
151. `[W8-151]` body를 raw SQL과 JSON 둘 다 허용하는가? 결과: `http_extract_sql`. 판정: `PASS`
152. `[W8-152]` README 문서와 실제 body 형식이 일관적인가? 결과: 불일치 흔적 존재. 판정: `FAIL`
153. `[W8-153]` thread pool 구조체 존재 여부. 결과: [`include/thread_pool.h`](../include/thread_pool.h:1). 판정: `PASS`
154. `[W8-154]` worker threads를 미리 생성하는가? 결과: `thread_pool_init`. 판정: `PASS`
155. `[W8-155]` bounded queue 사용 여부. 결과: ring buffer. 판정: `PASS`
156. `[W8-156]` main thread가 `accept()`만 담당하는가? 결과: 구조상 그렇다. 판정: `PASS`
157. `[W8-157]` worker가 `handle_client()`를 호출하는가? 결과: callback 구조. 판정: `PASS`
158. `[W8-158]` queue 보호에 mutex 사용 여부. 결과: `pthread_mutex_t`. 판정: `PASS`
159. `[W8-159]` queue empty/full 대기에 condvar 사용 여부. 결과: 두 개 사용. 판정: `PASS`
160. `[W8-160]` worker 종료 시 join 수행 여부. 결과: `thread_pool_shutdown`. 판정: `PASS`
161. `[W8-161]` DB 보호에 `pthread_rwlock_t` 사용 여부. 결과: [`include/db.h`](../include/db.h:46). 판정: `PASS`
162. `[W8-162]` SELECT가 read lock을 사용하는가? 결과: `pthread_rwlock_rdlock`. 판정: `PASS`
163. `[W8-163]` INSERT가 write lock을 사용하는가? 결과: `pthread_rwlock_wrlock`. 판정: `PASS`
164. `[W8-164]` 읽기끼리 병렬 허용 구조인가? 결과: 설계상 그렇다. 판정: `PASS`
165. `[W8-165]` 쓰기 중 읽기가 대기하는 구조인가? 결과: 설계상 그렇다. 판정: `PASS`
166. `[W8-166]` DB 전역 lock이 coarse-grained인가? 결과: 그렇다. 판정: `PASS`
167. `[W8-167]` 전역 lock이라 병목 가능성이 있는가? 결과: 있음. 판정: `PARTIAL`
168. `[W8-168]` API와 DB 엔진 경계가 분명한가? 결과: `server/sql/db` 분리. 판정: `PASS`
169. `[W8-169]` SQL parser와 API server가 느슨하게 결합되어 있는가? 결과: `execute_statement` 중간 계층 존재. 판정: `PASS`
170. `[W8-170]` 신호 처리로 종료 플래그를 세우는가? 결과: `SIGINT/SIGTERM`. 판정: `PASS`
171. `[W8-171]` SIGPIPE 무시 여부. 결과: 구현됨. 판정: `PASS`
172. `[W8-172]` `SO_REUSEADDR` 사용 여부. 결과: 구현됨. 판정: `PASS`
173. `[W8-173]` `accept()`의 `EINTR` 처리가 있는가? 결과: 있음. 판정: `PASS`
174. `[W8-174]` 요청 후 client fd를 닫는가? 결과: `close(client_fd)`. 판정: `PASS`
175. `[W8-175]` 종료 시 listen fd를 닫는가? 결과: 있음. 판정: `PASS`
176. `[W8-176]` 로그에 thread id와 fd가 남는가? 결과: `log_request`. 판정: `PASS`
177. `[W8-177]` 로그에 index_used와 elapsed_us가 남는가? 결과: 남음. 판정: `PASS`
178. `[W8-178]` `Content-Length` 기반 body 읽기 구현 여부. 결과: 있음. 판정: `PASS`
179. `[W8-179]` body가 너무 크면 400 처리하는가? 결과: 있음. 판정: `PASS`
180. `[W8-180]` full HTTP parser 수준인가? 결과: 최소 구현. 판정: `PARTIAL`
181. `[W8-181]` full JSON parser 수준인가? 결과: 최소 구현. 판정: `PARTIAL`
182. `[W8-182]` JSON unicode escape 지원 여부. 결과: 없음. 판정: `FAIL`
183. `[W8-183]` chunked transfer encoding 지원 여부. 결과: 없음. 판정: `FAIL`
184. `[W8-184]` HTTPS 지원 여부. 결과: 없음, 범위 밖. 판정: `PASS`
185. `[W8-185]` API 사용 예시가 README에 있는가? 결과: 있음. 판정: `PASS`
186. `[W8-186]` 외부 클라이언트 앱/프론트엔드가 있는가? 결과: 없음. 판정: `PARTIAL`
187. `[W8-187]` server integration test가 존재하는가? 결과: 없음. 판정: `FAIL`
188. `[W8-188]` thread pool 동시성 테스트가 존재하는가? 결과: 없음. 판정: `FAIL`
189. `[W8-189]` HTTP status code 회귀 테스트가 존재하는가? 결과: 없음. 판정: `FAIL`
190. `[W8-190]` malformed HTTP request 테스트가 존재하는가? 결과: 없음. 판정: `FAIL`
191. `[W8-191]` raw SQL body와 JSON body 모두를 문서와 테스트가 일관되게 다루는가? 결과: 테스트는 있으나 README 불일치. 판정: `PARTIAL`
192. `[W8-192]` API 서버 아키텍처가 교육용으로 설명 가능한가? 결과: 매우 설명하기 좋음. 판정: `PASS`
193. `[W8-193]` DB 엔진과 외부 API 연결 설계가 단순 명확한가? 결과: 명확함. 판정: `PASS`
194. `[W8-194]` 추가 구현 요소가 있는가? 결과: projection, raw SQL body, explicit id insert, benchmark 보강. 판정: `PASS`
195. `[W8-195]` 사용자 경험 측면에서 local REPL이나 CLI fallback이 있는가? 결과: 없음. 판정: `FAIL`
196. `[W8-196]` Windows 현재 환경에서 바로 빌드 가능한가? 결과: POSIX 의존, 현재 환경에서 어려움. 판정: `PARTIAL`
197. `[W8-197]` 현재 환경에서 실제 서버 실행 검증 가능한가? 결과: compiler 부재. 판정: `BLOCKED`
198. `[W8-198]` 현재 환경에서 `make test` 실행 검증 가능한가? 결과: compiler 부재. 판정: `BLOCKED`
199. `[W8-199]` 8주차 핵심인 API 서버, thread pool, DB 연동은 달성했는가? 결과: 달성. 판정: `PASS`
200. `[W8-200]` 8주차 총평. 결과: 구조는 좋지만 통합 검증과 사용성 문서가 약하다. 판정: `PARTIAL`

## 판정 요약

### 왜 PASS가 많은가
코어 구현은 생각보다 잘 되어 있다. 특히 parser, DB, B+ 트리, HTTP, thread pool이 모듈 단위로 분리되어 있어 읽기 쉽고, 기능 연결도 명확하다.

### 왜 FAIL도 적지 않은가
FAIL의 상당수는 "코드가 전혀 엉망이다"가 아니라 "원래 과제 산출물 형태와 다르다"에서 나온다. 가장 대표적인 것이 6주차 CLI SQL 처리기 부재다. 또 7주차 대규모 성능 검증 부족도 명확한 약점이다.

### 왜 PARTIAL이 많은가
교육용 프로젝트로서는 꽤 좋지만, 주차별 과제 원문에 딱 맞춰 보면 절충한 부분이 많다. 예를 들어 6주차는 CLI, 8주차는 API 서버인데, 현재 결과물은 후자 중심으로 흡수되었다. 7주차도 인덱스 구조는 좋지만 실험 설계가 약하다.

## 우선순위 개선 제안
1. 6주차 보완용 `sql_runner` CLI 바이너리를 추가한다.
2. 7주차 보완용 대량 데이터 생성기와 1,000,000건 benchmark 자동화를 넣는다.
3. 8주차 보완용 서버 통합 테스트와 병렬 요청 테스트를 추가한다.
4. README를 실제 구현과 일치하게 정리한다.
5. Linux/WSL 빌드 가이드를 명확히 적는다.

## 최종 코멘트
이 프로젝트는 "아무것도 없는 상태에서 여기까지 구현한 결과물"로 보면 분명히 칭찬할 부분이 많다. 구조가 선명하고, B+ 트리와 API 서버의 연결도 잘 되어 있으며, 동시성 제어도 최소한의 수준은 확보했다.

하지만 학생 평가 기준에서는 냉정해야 한다. 6주차 CLI 요구사항은 놓쳤고, 7주차 대용량 실험은 미흡하며, 8주차는 통합 검증이 약하다.

따라서 이 결과물은:

> 좋은 코드 구조를 가진 과제 결과물이지만, 6주차~8주차 원 요구사항을 모두 완전히 충족한 만점 결과물은 아니다.
