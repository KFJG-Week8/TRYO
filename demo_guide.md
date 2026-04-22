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
  발표자용 helper와 curl 요청 실행용입니다.
  외부 클라이언트 역할을 합니다.
```

서버는 최초 실행 전에 한 번 빌드해야 합니다. 터미널 A 또는 B 아무 곳에서나 프로젝트 루트에서 다음 명령을 실행합니다.

```sh
make
```

빌드가 끝나면 `./bin/week8_dbms` 서버 실행 파일이 만들어집니다.

서버를 여는 기본 명령은 터미널 A에서 실행합니다.

```sh
./bin/week8_dbms 8080 4 data/demo_users.csv
```

인자 의미:

```text
8080
  서버 포트입니다. curl 요청도 같은 포트를 사용해야 합니다.

4
  worker thread 수입니다.

data/demo_users.csv
  시연에 사용할 데이터 파일입니다.
```

서버가 정상적으로 열리면 터미널 A에 다음과 비슷한 로그가 출력됩니다.

```text
WEEK8 mini DBMS API server listening on http://127.0.0.1:8080 with 4 worker threads
Data file: data/demo_users.csv
```

터미널 B에서 서버가 열렸는지 확인합니다.

```sh
curl http://127.0.0.1:8080/health
```

예상 응답:

```json
{"status":"ok"}
```

서버를 끌 때는 터미널 A에서 `Ctrl+C`를 누릅니다.

시연은 두 가지 방식 중 하나로 진행할 수 있습니다.

```text
추천 방식
  scripts/demo_presenter.sh를 사용합니다.
  명령을 한 단계씩 화면에 보여주고 Enter를 누를 때만 실행하므로,
  메모장을 왔다 갔다 하지 않아도 직접 시연하는 느낌을 유지할 수 있습니다.

완전 수동 방식
  아래 단계에 적힌 curl 명령을 직접 복사해서 실행합니다.
  문제가 생겼을 때 개별 명령을 다시 실행하기 좋습니다.
```

주의할 점:

```text
scripts/concurrency_demo.sh는 병렬 요청을 한 번에 보내는 데모용 자동화입니다.
이 단계는 사람이 손으로 40개의 요청을 동시에 보낼 수 없으므로 사용하는 것이 자연스럽습니다.

반면 전체 시연을 처음부터 끝까지 조용히 자동 실행하면 설명할 지점이 줄어듭니다.
따라서 전체 흐름은 scripts/demo_presenter.sh로 한 단계씩 멈춰 가며 진행하는 것을 추천합니다.
```

## 추천 시연 방식: 발표자용 helper 사용

터미널을 2개 열고 다음처럼 사용합니다.

```text
터미널 A
  서버를 실행하고 worker thread 로그를 보여줍니다.

터미널 B
  scripts/demo_presenter.sh를 실행합니다.
  각 단계의 명령을 보여준 뒤 Enter를 누르면 실행합니다.
```

터미널 B에서 helper를 시작합니다.

```sh
bash scripts/demo_presenter.sh 8080 4 data/demo_users.csv
```

인자 의미:

```text
8080
  서버 포트입니다.

4
  worker thread 수입니다.

data/demo_users.csv
  시연용 데이터 파일입니다.
```

helper가 보여주는 동작:

```text
Enter
  현재 단계의 명령을 실행합니다.

s 입력 후 Enter
  현재 단계를 건너뜁니다.

서버 실행 단계
  터미널 A에서 직접 실행할 명령을 보여주고 기다립니다.
```

발표 멘트:

```text
시연 중에 사용할 명령들은 발표 흐름이 끊기지 않도록 helper에 모아두었습니다.
하지만 각 단계는 제가 Enter를 눌러 직접 실행하고,
화면에 실제 curl 요청과 서버 로그를 확인하면서 설명하겠습니다.
병렬 요청 부분만 여러 요청을 동시에 보내야 하므로 별도 스크립트로 실행합니다.
```

시연 중에는 아래 1단계부터 9단계까지의 흐름에 예외 처리 확인을 더해 설명하면 됩니다. helper는 이 순서대로 명령을 보여주고 실행합니다.

helper 진행 순서:

```text
1. make test로 내부 기능 확인
2. data/demo_users.csv 삭제 여부 확인
3. 터미널 A에서 서버 실행
4. GET /health 호출
5. POST /query로 INSERT 실행
6. POST /query로 SELECT 컬럼 조회 실행
7. concurrency_demo.sh로 병렬 INSERT 실행
8. parallel 데이터 조회
9. 지원하지 않는 SQL 에러 확인
10. 잘못된 SQL 문법 에러 확인
```

발표 중에는 helper가 명령을 보여줄 때 바로 Enter를 누르지 말고, 먼저 그 단계에서 보여줄 포인트를 짧게 말한 뒤 실행하면 자연스럽습니다.

예시 흐름:

```text
명령 확인
  "이번에는 외부 클라이언트가 /health API를 호출하는지 보겠습니다."

Enter 실행
  curl 결과를 확인합니다.

터미널 A 확인
  worker thread 로그에 thread id가 찍히는지 보여줍니다.
```

## 완전 수동 시연 방식

helper를 쓰지 않는다면 아래 명령을 순서대로 직접 실행하면 됩니다.

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
SQL parser, B+ tree, DB insert/select/reload, raw SQL body 추출, SELECT 컬럼 projection 테스트가 통과합니다.
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
  -H 'Content-Type: text/plain' \
  --data "INSERT INTO users VALUES (1, 'kim', 20);"
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
서버는 HTTP body에 들어온 SQL 문장을 그대로 꺼내고, SQL parser가 SqlStatement로 바꾼 뒤,
DB engine의 insert 작업으로 연결합니다.
```

## 5단계: API를 통해 SELECT 실행

터미널 B:

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: text/plain' \
  --data "SELECT id, name FROM users;"
```

예상 결과:

```json
{"ok":true,"rows":[{"id":1,"name":"kim"}],"message":"selected 1 row(s)","index_used":false,"elapsed_us":...}
```

보여줄 포인트:

```text
INSERT한 row를 HTTP API로 다시 조회합니다.
`SELECT id, name FROM users;`처럼 필요한 컬럼만 요청할 수 있습니다.
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
[thread=6096531456 fd=4] POST /query ok elapsed_us=129 index_used=false
[thread=6097104896 fd=4] POST /query ok elapsed_us=115 index_used=false
[thread=6097678336 fd=4] POST /query ok elapsed_us=93 index_used=false
[thread=6095958016 fd=5] POST /query ok elapsed_us=200 index_used=false
```

보여줄 포인트:

```text
서버 로그의 thread 값이 여러 종류로 찍히면 여러 worker thread가 요청을 나누어 처리한 것입니다.
fd 값은 같은 숫자가 반복될 수 있습니다. fd는 요청 번호가 아니라 OS가 재사용하는 연결 번호입니다.
요청은 병렬로 들어오지만 DB INSERT는 write lock으로 보호됩니다.
```

### 서버 로그 읽는 법

서버 로그는 아래 형식으로 찍힙니다.

```text
[thread=6097104896 fd=4] POST /query ok elapsed_us=115 index_used=false
```

각 값의 의미:

```text
thread=6097104896
  요청을 처리한 worker thread id입니다.
  병렬 처리 시연에서는 이 값이 여러 종류로 나오는지 보는 것이 핵심입니다.

fd=4
  accept()가 만든 client socket file descriptor 번호입니다.
  요청 번호나 사용자 번호가 아닙니다.

POST /query
  HTTP method와 route입니다.

ok
  요청 처리 결과입니다.
  bad-body, bad-sql, db-error 같은 값이 나오면 어느 단계에서 실패했는지 볼 수 있습니다.

elapsed_us=115
  DB 작업에 걸린 시간입니다. 단위는 microsecond입니다.

index_used=false
  SELECT WHERE id처럼 B+ tree index를 쓴 경우 true가 됩니다.
  INSERT나 name 검색, 전체 조회는 false입니다.
```

`fd=4`가 계속 반복되어도 정상입니다.

```text
서버는 요청 하나를 처리한 뒤 client_fd를 close합니다.
운영체제는 닫힌 fd 번호를 다음 연결에 다시 줄 수 있습니다.
그래서 서로 다른 요청인데도 fd=4가 반복될 수 있습니다.
```

예를 들어 아래 로그는 정상입니다.

```text
[thread=6095958016 fd=4] GET /health ok elapsed_us=0 index_used=false
[thread=6096531456 fd=4] GET /health ok elapsed_us=0 index_used=false
[thread=6097104896 fd=4] POST /query ok elapsed_us=582 index_used=false
[thread=6097678336 fd=4] POST /query ok elapsed_us=6 index_used=false
[thread=6097104896 fd=5] POST /query ok elapsed_us=200 index_used=false
```

이 로그에서 확인할 점:

```text
thread 값이 여러 개입니다.
  여러 worker thread가 요청을 나누어 처리하고 있습니다.

fd=4가 반복됩니다.
  닫힌 fd 번호를 OS가 재사용한 것입니다. 문제가 아닙니다.

fd=5도 가끔 보입니다.
  동시에 열린 연결이 겹치면 4번 외에 다른 fd도 할당될 수 있습니다.

POST /query ok가 계속 나옵니다.
  병렬 SQL 요청들이 실패하지 않고 정상 처리된 것입니다.
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
  -H 'Content-Type: text/plain' \
  --data "SELECT * FROM users WHERE name = 'parallel40';"
```

예상 결과:

```json
{"ok":true,"rows":[{"id":...,"name":"parallel40","age":20}],"message":"selected 1 row(s)","index_used":false,"elapsed_us":...}
```

주의:

```text
scripts/concurrency_demo.sh는 충돌을 피하려고 id를 10000번대부터 넣습니다.
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
  -H 'Content-Type: text/plain' \
  --data "DELETE FROM users WHERE id = 1;"
```

예상 결과:

```json
{"ok":false,"error":"only INSERT and SELECT are supported"}
```

### 잘못된 SQL 문법

터미널 B:

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: text/plain' \
  --data "INSERT INTO users VALUES ('broken', 20);"
```

예상 결과:

```json
{"ok":false,"error":"expected positive id"}
```

발표 멘트:

```text
지원하지 않는 SQL이나 잘못된 SQL 문법이 들어와도 서버가 죽지 않고 JSON error를 반환합니다.
production 수준의 완전한 HTTP/SQL parser는 아니지만,
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

### fd가 계속 같은 숫자로 찍힐 때

증상:

```text
[thread=...] POST /query ok ... fd=4
[thread=...] POST /query ok ... fd=4
[thread=...] POST /query ok ... fd=4
```

판단:

```text
정상입니다.
fd는 요청 고유 번호가 아니라 운영체제가 관리하는 file descriptor 번호입니다.
서버가 요청 처리 후 close(fd)를 하면, 다음 accept()에서 같은 번호가 다시 나올 수 있습니다.
```

발표 답변:

```text
fd=4가 반복되는 것은 같은 연결을 계속 쓴다는 뜻이 아닙니다.
요청마다 연결이 닫히고, OS가 비어 있는 fd 번호를 재사용한 것입니다.
thread 값이 여러 종류로 찍히는지가 thread pool 병렬 처리 확인 포인트입니다.
```

### thread 값이 하나만 보일 때

원인:

```text
요청이 너무 빨리 끝나서 한 worker가 대부분 처리했을 수 있습니다.
또는 병렬 요청 수가 worker 수보다 적을 수 있습니다.
```

대처:

```sh
bash scripts/concurrency_demo.sh 8080 100 30
```

판단:

```text
thread 값이 여러 종류로 늘어나면 thread pool이 여러 worker에게 요청을 분배한 것입니다.
그래도 fd 값은 같은 숫자가 반복될 수 있습니다.
```

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

helper를 쓰는 경우에는 처음 실행할 때 포트를 8081로 넘깁니다.

```sh
bash scripts/demo_presenter.sh 8081 4 data/demo_users.csv
```

### duplicate id가 나올 때

원인:

```text
data/demo_users.csv에 이전 시연 데이터가 남아 있어서 같은 id를 다시 INSERT했을 수 있습니다.
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
