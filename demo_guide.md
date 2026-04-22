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
  서버 실행 상태 확인용입니다.

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
------------------------------------------------------------
WEEK8 mini DBMS API server
------------------------------------------------------------
Listening   http://127.0.0.1:8080
Workers     4
Data file   data/demo_users.csv
Stop        Ctrl+C
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
  서버를 실행해 둡니다.

터미널 B
  scripts/demo_presenter.sh를 실행합니다.
  각 단계의 명령을 보여준 뒤 Enter를 누르면 실행합니다.
```

깨끗한 시연 데이터로 시작하려면 서버를 켜기 전에 터미널 A 또는 B에서 먼저 실행합니다. 이미 서버를 켠 뒤에는 이 명령을 실행하지 않습니다.

```sh
rm -f data/demo_users.csv
```

그 다음 터미널 A에서 서버를 실행하고, `/health` 응답까지 확인한 뒤 helper를 시작합니다.

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
```

발표 멘트:

```text
시연 중에 사용할 명령들은 발표 흐름이 끊기지 않도록 helper에 모아두었습니다.
하지만 각 단계는 제가 Enter를 눌러 직접 실행하고,
화면에 실제 curl 요청과 클라이언트 응답을 확인하면서 설명하겠습니다.
병렬 요청 부분만 여러 요청을 동시에 보내야 하므로 별도 스크립트로 실행합니다.
```

시연 중에는 아래 흐름대로 설명하면 됩니다. helper는 이미 실행 중인 서버에 대해 이 순서대로 명령을 보여주고 실행합니다.

번호는 helper 출력과 맞춰 1번부터 순서대로 이어집니다.

helper 진행 순서:

```text
1. GET /health 호출
2. POST /query로 INSERT 실행
3. POST /query로 SELECT 컬럼 조회 실행
4. concurrency_demo.sh로 병렬 INSERT 실행 및 client-side thread id 확인
5. parallel 데이터 조회로 저장 상태 확인
```

발표 중에는 helper가 명령을 보여줄 때 바로 Enter를 누르지 말고, 먼저 그 단계에서 보여줄 포인트를 짧게 말한 뒤 실행하면 자연스럽습니다.

예시 흐름:

```text
명령 확인
  "이번에는 외부 클라이언트가 /health API를 호출하는지 보겠습니다."

Enter 실행
  curl 결과를 확인합니다.
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

## 2단계: 서버 실행

터미널 A:

```sh
./bin/week8_dbms 8080 4 data/demo_users.csv
```

예상 결과:

```text
------------------------------------------------------------
WEEK8 mini DBMS API server
------------------------------------------------------------
Listening   http://127.0.0.1:8080
Workers     4
Data file   data/demo_users.csv
Stop        Ctrl+C
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

보여줄 포인트:

```text
curl은 외부 HTTP 클라이언트 역할을 합니다.
서버는 HTTP route를 처리하고 JSON 응답을 반환합니다.
```

발표 멘트:

```text
여기서 curl이 외부 클라이언트입니다.
클라이언트는 서버 내부 파일이나 B+ 트리를 직접 만지지 않고,
HTTP API만 사용해서 서버와 통신합니다.
```

## 4단계: API로 단일 INSERT와 SELECT 확인

터미널 B:

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: text/plain' \
  --data "INSERT INTO users VALUES (1, 'kim', 20);"
```

예상 결과:

```json
[{"id":1,"name":"kim","age":20}]
```

이어서 방금 넣은 row를 필요한 컬럼만 골라 조회합니다.

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: text/plain' \
  --data "SELECT id, name FROM users;"
```

예상 결과:

```json
[{"id":1,"name":"kim"}]
```

보여줄 포인트:

```text
외부 클라이언트는 POST /query에 SQL 문자열을 담아 보냅니다.
API 서버는 SQL 문자열을 parser로 넘기고, 내부 DB engine의 INSERT 또는 SELECT로 연결합니다.
`SELECT id, name FROM users;`처럼 필요한 컬럼만 요청할 수 있습니다.
```

발표 멘트:

```text
이 요청들은 외부 클라이언트가 DBMS 기능을 사용하는 예시입니다.
클라이언트는 HTTP body에 SQL 문장을 담아 보내고,
서버는 parser와 DB engine으로 연결해 INSERT와 SELECT 결과를 JSON으로 돌려줍니다.
```

## 5단계: 클라이언트 출력으로 thread pool 병렬 INSERT 시연

이 단계가 이번 시연의 핵심입니다.

테스트 목적:

```text
이 테스트는 "여러 외부 클라이언트가 동시에 HTTP 요청을 보내도 서버가 요청을 잃지 않고 처리하는지"를 확인하기 위한 테스트입니다.

구체적으로는 세 가지를 봅니다.
1. main thread가 accept한 여러 client fd가 thread pool queue로 들어간다.
2. 여러 worker thread가 queue에서 요청을 꺼내 POST /query를 나누어 처리한다.
3. 동시에 INSERT가 들어와도 DB engine의 write lock 덕분에 records 배열, data file, B+ tree index가 깨지지 않는다.
```

성공 기준:

```text
Successful responses가 40 / 40이면 40개의 병렬 INSERT 요청이 모두 정상 처리된 것입니다.
Worker thread distribution에 thread 값이 여러 종류로 보이면 worker thread들이 요청을 나누어 처리한 것입니다.
6단계 SELECT에서 방금 넣은 row가 조회되면 동시 INSERT 이후에도 DB 상태가 유지된 것입니다.
```

서버는 각 JSON 응답에 `X-Worker-Thread-Id` 헤더를 함께 내려줍니다. `scripts/concurrency_demo.sh`는 이 헤더를 모아서 터미널 B에 요약하므로, 병렬 처리 확인을 위해 서버 로그를 왔다 갔다 볼 필요가 없습니다.

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
------------------------------------------------------------
Concurrency demo
------------------------------------------------------------
Settings
  Base URL    http://127.0.0.1:8080
  Requests    40
  Parallel    12
  Run label   run123456
  Base id     123456789

------------------------------------------------------------
Health check
------------------------------------------------------------

Command pattern
  $ curl -s http://127.0.0.1:8080/health
{"status":"ok"}

------------------------------------------------------------
Parallel INSERT requests
------------------------------------------------------------

Command pattern
  $ curl -s -X POST http://127.0.0.1:8080/query -H 'Content-Type: text/plain' --data "INSERT INTO users VALUES (..., 'parallel_run123456_...', ...);"

Sending 40 INSERT requests with parallelism 12...

------------------------------------------------------------
Result
------------------------------------------------------------
Successful responses  40 / 40

Worker thread distribution returned to the client
-----------------------------------------------
  thread=6095958016  responses=9
  thread=6096531456  responses=11
  thread=6097104896  responses=10
  thread=6097678336  responses=10

Sample client-side INSERT responses
---------------
  request=1 row_id=123456790 thread=6096531456 body=[{"id":123456790,"name":"parallel_run123456_1","age":21}]
  request=2 row_id=123456791 thread=6097104896 body=[{"id":123456791,"name":"parallel_run123456_2","age":22}]

Thread ids above came from the X-Worker-Thread-Id response header.
```

보여줄 포인트:

```text
thread 값은 서버 로그가 아니라 클라이언트가 받은 HTTP 응답 헤더에서 온 값입니다.
thread 값이 여러 종류로 나오면 여러 worker thread가 INSERT 요청을 나누어 처리한 것입니다.
요청은 병렬로 들어오지만 DB INSERT는 write lock으로 보호됩니다.
Successful responses가 0 / 40이면 정상 시연 결과가 아닙니다. duplicate id 같은 에러가 나오면 이전 데이터와 충돌한 것이므로 서버를 다시 빌드하거나 demo 데이터를 비우고 다시 실행합니다.
```

thread 값이 거의 한 종류로만 보이면 요청이 너무 빨리 끝난 것일 수 있습니다. 그때는 요청 수와 병렬도를 올려 다시 실행합니다.

```sh
bash scripts/concurrency_demo.sh 8080 100 30
```

발표 멘트:

```text
이제 동시에 여러 SQL INSERT 요청을 보내겠습니다.
응답마다 어떤 worker thread가 처리했는지를 헤더로 받아서 클라이언트 화면에 모았습니다.
여러 thread id가 보이면 thread pool이 요청을 분배한 것이고,
동시에 INSERT가 들어와도 DB engine은 write lock으로 공유 상태를 보호합니다.
```

## 6단계: 병렬 INSERT 결과를 SELECT로 확인

5단계에서 thread 분배를 이미 클라이언트 출력으로 확인했습니다. 6단계는 thread id를 다시 보여주는 단계가 아니라, 병렬 INSERT 이후 실제 데이터가 DB에 저장되어 있는지 확인하는 단계로 둡니다.

터미널 B:

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: text/plain' \
  --data "SELECT * FROM users WHERE name = 'parallel_run123456_40';"
```

예상 결과:

```json
[{"id":...,"name":"parallel_run123456_40","age":20}]
```

주의:

```text
scripts/concurrency_demo.sh는 충돌을 피하려고 실행마다 Base id와 Run label을 새로 만듭니다.
helper를 사용할 때는 같은 Run label로 병렬 INSERT와 검증 SELECT를 이어서 실행합니다.
```

보여줄 포인트:

```text
5단계는 "여러 worker thread가 요청을 나누어 처리했다"는 것을 보여줍니다.
6단계는 "동시에 들어온 INSERT들이 DB 상태를 깨뜨리지 않고 저장됐다"는 것을 보여줍니다.
파일, records 배열, B+ tree index 갱신이 write lock 안에서 일관되게 처리됩니다.
```

발표 멘트:

```text
방금 병렬로 넣은 데이터 중 하나를 다시 조회해보겠습니다.
5단계에서 thread 분배를 확인했고,
여기서는 그 결과가 DB에 정상적으로 남아 있는지를 확인합니다.
```

## 마무리 멘트

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
