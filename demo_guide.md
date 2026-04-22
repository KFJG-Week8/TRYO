# WEEK8 미니 DBMS API 서버 시연 가이드

이번 주 과제의 핵심은 B+ 트리 자체를 다시 설명하는 것이 아니라, 이전 차수에서 만든 SQL 처리기와 B+ 트리 인덱스를 내부 DB engine으로 활용하면서 **외부 API 서버와 thread pool 기반 병렬 요청 처리**를 보여주는 것입니다.

시연 목표는 세 가지입니다.

```text
1. 외부 클라이언트가 HTTP API로 DBMS 기능을 사용할 수 있다.
2. thread pool이 요청을 worker thread에게 분배하고 SQL 요청을 병렬 처리한다.
3. 여러 worker가 같은 DB engine을 공유하므로 동시성 제어가 필요하고, 현재 코드는 lock으로 이를 처리한다.
```

## 시연 전 준비

터미널을 2개 엽니다.

```text
터미널 A
  서버 실행용입니다.
  worker thread 로그를 보여주는 화면입니다.

터미널 B
  curl 요청 실행용입니다.
  외부 클라이언트 역할을 합니다.
```

깨끗한 시연을 원하면 기존 demo 데이터를 삭제합니다. 이전 데이터를 보존해야 한다면 이 명령은 실행하지 않습니다.

```sh
rm -f data/demo_users.csv
```

발표 멘트:

```text
오늘 시연은 data/demo_users.csv라는 별도 파일로 진행하겠습니다.
이렇게 하면 기존 데이터와 섞이지 않고, id가 1부터 시작하는 것을 명확히 볼 수 있습니다.
```

## 1단계: 테스트로 내부 기능 확인

터미널 B:

```sh
make test
```

예상 결과:

```text
./bin/test_week8_dbms
All tests passed
```

보여줄 포인트:

```text
SQL parser, B+ tree, DB insert/select/reload, JSON body에서 SQL 추출 테스트가 통과합니다.
```

발표 멘트:

```text
먼저 내부 기능 테스트를 확인하겠습니다.
이번 주차의 중심은 API 서버와 thread pool이지만,
이 서버가 호출하는 내부 SQL 처리기와 DB engine이 정상 동작해야 전체 시연이 의미가 있습니다.
```

## 2단계: 서버 실행

터미널 A:

```sh
./bin/week8_dbms 8080 4 data/demo_users.csv
```

예상 결과:

```text
WEEK8 mini DBMS API server listening on http://127.0.0.1:8080 with 4 worker threads
Data file: data/demo_users.csv
```

보여줄 포인트:

```text
포트: 8080
worker thread 수: 4
데이터 파일: data/demo_users.csv
```

발표 멘트:

```text
서버를 worker thread 4개로 실행하겠습니다.
main thread는 accept loop에서 연결을 받고,
실제 HTTP 요청 처리와 SQL 실행은 worker thread들이 나눠 맡습니다.
```

## 3단계: 외부 클라이언트가 API를 호출하는지 확인

터미널 B:

```sh
curl http://127.0.0.1:8080/health
```

예상 결과:

```json
{"status":"ok"}
```

터미널 A 서버 로그 예상:

```text
[thread=... fd=...] GET /health ok elapsed_us=0 index_used=false
```

보여줄 포인트:

```text
curl은 외부 HTTP 클라이언트 역할을 합니다.
서버는 HTTP route를 처리하고 JSON 응답을 반환합니다.
서버 로그에 worker thread id가 찍힙니다.
```

발표 멘트:

```text
여기서 curl이 외부 클라이언트입니다.
클라이언트는 서버 내부 파일이나 B+ 트리를 직접 만지지 않고,
HTTP API만 사용해서 서버와 통신합니다.
```

## 4단계: API를 통해 INSERT 실행

터미널 B:

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data '{"sql":"INSERT INTO users name age VALUES '\''kim'\'' 20;"}'
```

예상 결과:

```json
{"ok":true,"rows":[{"id":1,"name":"kim","age":20}],"message":"inserted 1 row","index_used":false,"elapsed_us":...}
```

터미널 A 서버 로그 예상:

```text
[thread=... fd=...] POST /query ok elapsed_us=... index_used=false
```

보여줄 포인트:

```text
외부 클라이언트는 POST /query에 SQL 문자열을 담아 보냅니다.
API 서버는 SQL 문자열을 parser로 넘기고, 내부 DB engine의 INSERT로 연결합니다.
```

발표 멘트:

```text
이 요청은 외부 클라이언트가 DBMS 기능을 사용하는 예시입니다.
서버는 HTTP body에서 sql 값을 꺼내고, SQL parser가 SqlStatement로 바꾼 뒤,
DB engine의 insert 작업으로 연결합니다.
```

## 5단계: API를 통해 SELECT 실행

터미널 B:

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data '{"sql":"SELECT * FROM users;"}'
```

예상 결과:

```json
{"ok":true,"rows":[{"id":1,"name":"kim","age":20}],"message":"selected 1 row(s)","index_used":false,"elapsed_us":...}
```

보여줄 포인트:

```text
INSERT한 row를 HTTP API로 다시 조회합니다.
외부 클라이언트 입장에서는 DBMS 기능을 API로 사용하고 있습니다.
```

발표 멘트:

```text
방금 삽입한 데이터를 다시 API로 조회했습니다.
이 시점에서 외부 API 서버와 내부 DB engine 연결이 동작한다는 것을 확인할 수 있습니다.
```

## 6단계: thread pool 병렬 요청 시연

이 단계가 이번 시연의 핵심입니다.

터미널 B:

```sh
bash scripts/concurrency_demo.sh 8080 40 12
```

인자 의미:

```text
8080
  서버 포트입니다.

40
  동시에 보낼 INSERT 요청 총 개수입니다.

12
  한 번에 병렬로 실행할 curl 개수입니다.
```

예상 결과:

```text
Health:
{"status":"ok"}

Sending 40 INSERT requests with parallelism 12...
Successful responses: 40 / 40

Sample response:
{"ok":true,"rows":[{"id":...,"name":"parallel...","age":...}],"message":"inserted 1 row","index_used":false,"elapsed_us":...}

Done. Check the server terminal for multiple [thread=...] values.
```

터미널 A 서버 로그 예상:

```text
[thread=... fd=...] POST /query ok elapsed_us=... index_used=false
[thread=... fd=...] POST /query ok elapsed_us=... index_used=false
[thread=... fd=...] POST /query ok elapsed_us=... index_used=false
```

보여줄 포인트:

```text
서버 로그의 thread 값이 여러 종류로 찍히면 여러 worker thread가 요청을 나누어 처리한 것입니다.
요청은 병렬로 들어오지만 DB INSERT는 write lock으로 보호됩니다.
```

thread 값이 거의 한 종류로만 보이면 요청이 너무 빨리 끝난 것일 수 있습니다. 그때는 요청 수와 병렬도를 올려 다시 실행합니다.

```sh
bash scripts/concurrency_demo.sh 8080 100 30
```

발표 멘트:

```text
이제 동시에 여러 SQL 요청을 보내겠습니다.
요청 자체는 thread pool에 의해 여러 worker thread로 분배됩니다.
하지만 여러 worker가 같은 DbEngine을 공유하기 때문에 동시성 문제가 생길 수 있습니다.
그래서 thread pool queue는 mutex와 condition variable로 보호하고,
DB engine은 read-write lock으로 보호합니다.
특히 INSERT는 next_id, records 배열, 파일, B+ tree index를 함께 바꾸므로 write lock 안에서 실행됩니다.
```

## 7단계: 동시 INSERT 이후 데이터가 깨지지 않았는지 확인

터미널 B:

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data '{"sql":"SELECT * FROM users WHERE name = '\''parallel40'\'';"}'
```

예상 결과:

```json
{"ok":true,"rows":[{"id":...,"name":"parallel40","age":20}],"message":"selected 1 row(s)","index_used":false,"elapsed_us":...}
```

주의:

```text
id는 이전에 넣은 데이터 개수에 따라 달라질 수 있습니다.
name은 parallel40으로 고정되어 있으므로 동시 INSERT 결과 확인에 적합합니다.
```

보여줄 포인트:

```text
동시에 INSERT 요청을 보냈지만 row가 정상적으로 저장되어 있습니다.
파일, records 배열, B+ tree index 갱신이 write lock 안에서 일관되게 처리됩니다.
```

발표 멘트:

```text
동시 요청 이후 특정 row를 다시 조회해보겠습니다.
여기서 데이터가 정상적으로 조회된다는 것은,
여러 worker가 동시에 요청을 처리해도 DB 상태가 깨지지 않았다는 것을 보여줍니다.
```

## 8단계: 예외 처리도 API 응답으로 보여주기

예외 처리는 이번 과제의 메인은 아니지만, API 서버로서 기본 실패 응답을 보여주면 좋습니다.

### 지원하지 않는 SQL

터미널 B:

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data '{"sql":"DELETE FROM users WHERE id = 1;"}'
```

예상 결과:

```json
{"ok":false,"error":"only INSERT and SELECT are supported"}
```

### 잘못된 JSON key

터미널 B:

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data '{"query":"SELECT * FROM users;"}'
```

예상 결과:

```json
{"ok":false,"error":"JSON body must contain key \"sql\""}
```

발표 멘트:

```text
지원하지 않는 SQL이나 잘못된 JSON body가 들어와도 서버가 죽지 않고 JSON error를 반환합니다.
production 수준의 완전한 HTTP/JSON parser는 아니지만,
이번 과제 범위에서 필요한 기본 실패 케이스는 처리하고 있습니다.
```

## 9단계: 마무리 멘트

```text
오늘 시연의 핵심은 세 가지입니다.

첫째, 외부 클라이언트가 HTTP API를 통해 DBMS 기능을 사용할 수 있습니다.
둘째, API 서버는 SQL 문자열을 SQL 처리기와 내부 DB engine으로 연결합니다.
셋째, thread pool을 통해 여러 요청을 worker thread들이 병렬로 처리하고,
공유 DB 상태는 read-write lock으로 보호합니다.

이전 차수의 B+ tree 인덱스는 내부 DB engine의 일부로 재사용했고,
이번 주차에서는 그 DB engine을 외부 API 서버와 멀티 스레드 요청 처리 구조에 연결했습니다.
```

## 실패했을 때 빠른 대처

### 포트가 이미 사용 중일 때

증상:

```text
bind: Address already in use
```

대처:

```sh
./bin/week8_dbms 8081 4 data/demo_users.csv
```

curl과 script의 포트도 8081로 바꿉니다.

```sh
curl http://127.0.0.1:8081/health
bash scripts/concurrency_demo.sh 8081 40 12
```

### id가 예상과 다를 때

원인:

```text
data/demo_users.csv에 이전 시연 데이터가 남아 있을 수 있습니다.
```

대처:

```text
서버를 Ctrl+C로 종료합니다.
data/demo_users.csv를 삭제합니다.
서버를 다시 실행합니다.
```

명령:

```sh
rm -f data/demo_users.csv
./bin/week8_dbms 8080 4 data/demo_users.csv
```

### curl이 연결되지 않을 때

확인할 것:

```text
서버가 켜져 있는지 확인합니다.
port가 맞는지 확인합니다.
다른 포트로 실행했다면 curl URL도 같은 포트로 바꿉니다.
```

### 서버를 종료할 때

터미널 A:

```text
Ctrl+C
```

예상 결과:

```text
Server stopped
```
