# WEEK8 미니 DBMS API 서버 면접 대비 답변 60개

## API 서버 아키텍처

### Q1. 이 프로젝트에서 API 서버는 정확히 어떤 역할을 하나요?

답변:
이 프로젝트의 API 서버는 외부 클라이언트와 내부 DBMS 사이의 진입점 역할을 합니다. 클라이언트가 HTTP로 SQL을 보내면 서버가 요청을 읽고, 라우팅하고, SQL 파서와 DB engine을 호출한 뒤 JSON 응답을 돌려줍니다. 즉 DB engine이 네트워크를 직접 알지 않도록 HTTP 계층과 DB 계층을 연결하는 어댑터 역할을 합니다.

### Q2. `main.c`, `server.c`, `http.c`, `sql.c`, `db.c`, `bptree.c`는 각각 어떤 책임을 갖나요?

답변:
`main.c`는 port, thread 개수, data file 같은 CLI 인자를 읽어 `ServerConfig`를 만듭니다. `server.c`는 socket 생성, accept loop, 라우팅, worker handler, SQL 실행 연결을 담당합니다. `http.c`는 HTTP request를 읽고 SQL body를 추출하며 JSON response를 보냅니다. `sql.c`는 지원 SQL을 `SqlStatement`로 파싱하고, `db.c`는 파일 기반 users table과 lock, insert/select 실행을 맡고, `bptree.c`는 `id -> record_index` 인덱스를 관리합니다.

### Q3. 클라이언트가 `POST /query`를 보냈을 때 서버 내부 흐름을 처음부터 끝까지 설명해보세요.

답변:
먼저 main thread가 `accept()`로 client socket을 받고 thread pool queue에 `client_fd`를 넣습니다. worker thread가 queue에서 fd를 꺼낸 뒤 `http_read_request()`로 HTTP 요청을 읽고, route가 `POST /query`인지 확인합니다. 이후 `http_extract_sql()`로 body에서 SQL을 꺼내고 `sql_parse()`로 `SqlStatement`를 만든 다음 `execute_statement()`가 `db_insert()` 또는 `db_select_projected()`를 호출합니다. 마지막으로 결과를 JSON으로 응답하고 client fd를 닫습니다.

### Q4. `socket() -> bind() -> listen() -> accept()` 흐름에서 각 단계는 어떤 역할을 하나요?

답변:
`socket()`은 TCP 통신에 사용할 file descriptor를 만드는 단계입니다. `bind()`는 그 socket을 특정 port와 IP 주소에 연결합니다. `listen()`은 해당 socket을 서버용 listening socket으로 전환해 연결 요청을 받을 준비를 합니다. `accept()`는 실제 클라이언트 연결 하나를 꺼내 새로운 `client_fd`를 반환합니다.

### Q5. `listen_fd`와 `client_fd`의 차이는 무엇인가요?

답변:
`listen_fd`는 서버가 계속 열어두는 문지기 같은 socket으로, 새 연결을 받기 위해 사용됩니다. 반면 `client_fd`는 `accept()`가 반환하는 개별 클라이언트 연결용 socket입니다. 이 프로젝트에서는 main thread가 `listen_fd`로 연결을 받고, worker thread는 전달받은 `client_fd`로 요청과 응답을 처리합니다.

### Q6. 왜 main thread가 요청을 직접 처리하지 않고 worker thread에게 넘기나요?

답변:
main thread가 요청 처리까지 직접 하면 SQL 실행이나 파일 접근이 오래 걸릴 때 새 연결을 받지 못합니다. 그래서 main thread는 accept에 집중하고, 실제 요청 처리는 worker thread가 병렬로 수행하게 했습니다. 이렇게 하면 여러 클라이언트 요청을 동시에 처리할 수 있고, thread 생성 비용도 thread pool로 줄일 수 있습니다.

### Q7. `GET /health`와 `POST /query`를 분리한 이유는 무엇인가요?

답변:
`GET /health`는 서버가 살아 있는지 확인하는 가벼운 상태 확인 API이고, `POST /query`는 SQL 실행이라는 실제 DB 작업 API입니다. 두 기능의 목적과 부하가 다르기 때문에 route를 분리하면 시연과 테스트가 명확해집니다. 또한 health check는 body 없이 호출할 수 있어 서버 실행 여부를 빠르게 검증할 수 있습니다.

### Q8. 이 서버가 HTTP keep-alive를 지원하지 않고 요청마다 연결을 닫는 이유는 무엇인가요?

답변:
프로젝트 범위를 미니 DBMS API 서버 학습에 맞추기 위해 HTTP keep-alive는 의도적으로 제외했습니다. 현재 구현은 한 연결에서 한 요청을 읽고 한 응답을 보낸 뒤 `Connection: close`로 닫습니다. 이렇게 하면 request boundary 처리와 connection lifecycle이 단순해져 socket, thread pool, DB 동시성 같은 핵심 학습 포인트에 집중할 수 있습니다.

### Q9. 서버 로그에는 `127.0.0.1`로 출력되지만 실제 bind는 `INADDR_ANY`입니다. 이 차이는 무엇을 의미하나요?

답변:
`INADDR_ANY`는 서버가 모든 네트워크 인터페이스에서 들어오는 연결을 받을 수 있게 bind한다는 의미입니다. 반면 로그의 `127.0.0.1`은 로컬에서 테스트할 때 접속하기 쉬운 대표 주소를 보여주는 안내입니다. 즉 실제 서버는 더 넓게 열려 있지만, 시연 메시지는 localhost 기준으로 친절하게 표시한 것입니다.

### Q10. 다른 팀이 CLI 명령으로만 DBMS를 조작했다면, 이 프로젝트가 HTTP API 서버 구조를 택한 장단점은 무엇인가요?

답변:
장점은 DBMS 기능을 `curl`, 브라우저, 외부 프로그램 같은 HTTP 클라이언트에서 사용할 수 있다는 점입니다. 또한 thread pool과 네트워크 요청 처리를 통해 실제 서버 구조에 가까운 흐름을 보여줄 수 있습니다. 단점은 CLI보다 구현 범위가 넓어져 HTTP parsing, socket, 동시성, 오류 응답 같은 추가 복잡도를 관리해야 한다는 점입니다.

## 외부 클라이언트와 CLI 환경

### Q11. “외부 클라이언트에서 DBMS 기능을 사용할 수 있어야 한다”는 요구사항을 이 코드는 어떻게 만족하나요?

답변:
서버가 TCP socket을 열고 `GET /health`, `POST /query` HTTP API를 제공하기 때문에 외부 클라이언트는 HTTP 요청으로 DBMS 기능을 사용할 수 있습니다. 특히 `POST /query` body에 SQL을 담아 보내면 내부 SQL parser와 DB engine까지 연결됩니다. 그래서 DBMS가 단순 콘솔 프로그램이 아니라 네트워크를 통해 호출 가능한 서버가 됩니다.

### Q12. 별도의 `client.c`가 없어도 `curl`을 외부 클라이언트로 볼 수 있나요?

답변:
네, 볼 수 있습니다. 외부 클라이언트의 핵심은 서버 프로세스 밖에서 네트워크 요청을 보내는 주체라는 점이고, `curl`은 HTTP 요청을 생성하는 독립 실행 프로그램입니다. 따라서 별도 C 클라이언트를 만들지 않아도 `curl`로 API 요구사항을 충분히 시연할 수 있습니다.

### Q13. 외부 PC에서 이 서버에 접속하려면 코드 외에 어떤 네트워크 조건이 필요하나요?

답변:
외부 PC에서 접속하려면 서버가 실행 중인 PC의 실제 IP와 port에 접근할 수 있어야 합니다. 같은 네트워크에 있거나 라우팅이 가능해야 하고, OS 방화벽이나 공유기 방화벽에서 해당 port가 막혀 있지 않아야 합니다. 또한 현재 bind는 `INADDR_ANY`라 코드상으로는 외부 인터페이스 접속을 받을 준비가 되어 있지만, 네트워크 정책이 허용되어야 실제 접속이 됩니다.

### Q14. `./bin/week8_dbms [port] [thread_count] [data_file]` 형태의 CLI는 어떤 점에서 시연과 테스트에 유리한가요?

답변:
실행 시점에 port, worker 수, 데이터 파일을 바꿀 수 있어서 같은 바이너리로 여러 시나리오를 테스트할 수 있습니다. 예를 들어 port 충돌이 나면 다른 port를 쓰고, thread 수를 바꿔 동시성 차이를 확인할 수 있습니다. data file도 시연용과 테스트용을 분리할 수 있어 반복 실험이 쉬워집니다.

### Q15. port, thread_count, data_file을 실행 인자로 받을 수 있게 한 이유는 무엇인가요?

답변:
이 값들은 서버 실행 환경에 따라 달라질 가능성이 큰 설정값입니다. port는 로컬 충돌이나 배포 환경에 따라 바뀔 수 있고, thread_count는 동시 처리 실험에 영향을 줍니다. data_file은 데이터 영속성 확인이나 테스트 격리를 위해 바꿀 수 있어야 하므로 코드에 고정하지 않고 실행 인자로 받게 했습니다.

### Q16. 잘못된 port나 thread_count가 들어왔을 때 fallback 값을 쓰는 설계는 어떤 장단점이 있나요?

답변:
장점은 사용자가 잘못된 값을 넣어도 서버가 기본값으로 실행되어 시연이 끊기지 않는다는 점입니다. 특히 교육용 프로젝트에서는 실행 실패보다 빠른 확인이 중요할 수 있습니다. 단점은 사용자가 입력 실수를 즉시 알아차리기 어렵다는 점이라, 운영용이라면 경고 로그를 더 명확히 남기거나 잘못된 설정은 실패시키는 방식도 고려할 수 있습니다.

### Q17. port 기본값을 8080, worker thread 기본값을 4, data file 기본값을 `data/users.csv`로 둔 근거를 어떻게 설명하겠나요?

답변:
8080은 로컬 개발용 HTTP 서버에서 흔히 쓰는 port라 기억하기 쉽고 권한 문제도 적습니다. worker thread 4개는 작은 학습 프로젝트에서 병렬 처리 효과를 보여주면서도 과도한 자원을 쓰지 않는 무난한 값입니다. `data/users.csv`는 users 테이블이라는 프로젝트 범위와 파일 기반 저장 방식을 직관적으로 드러내는 기본 경로입니다.

### Q18. CLI 환경에서 `make`, 서버 실행, `curl`, `scripts/benchmark.sh`는 각각 어떤 역할을 하나요?

답변:
`make`는 서버와 테스트 바이너리를 빌드하는 역할을 합니다. 서버 실행은 실제 DBMS API 서버 프로세스를 띄우는 단계입니다. `curl`은 health check나 SQL query를 보내는 외부 HTTP 클라이언트 역할을 하고, `scripts/benchmark.sh`는 대량 INSERT, id 검색, name 검색, 동시 health 요청을 묶어 성능과 동작을 시연합니다.

## HTTP 처리와 예외 처리

### Q19. 이 서버는 HTTP request의 끝을 어떻게 판단하나요?

답변:
먼저 header의 끝은 HTTP 규칙대로 `\r\n\r\n` 문자열을 찾는 방식으로 판단합니다. 그 다음 `Content-Length`를 파싱해서 body 길이를 계산하고, header 길이와 body 길이를 합친 만큼 받을 때까지 `recv()`를 반복합니다. 즉 header boundary와 Content-Length를 함께 사용해 한 요청의 끝을 정합니다.

### Q20. TCP byte stream과 HTTP request message의 차이는 무엇인가요?

답변:
TCP는 byte stream이라서 한 번의 `recv()`가 HTTP 요청 하나와 정확히 대응된다는 보장이 없습니다. HTTP request message는 그 byte stream 위에 올라간 프로토콜 형식으로, request line, header, 빈 줄, body로 구성됩니다. 그래서 서버는 TCP에서 받은 바이트를 모아 HTTP 메시지 경계를 직접 찾아야 합니다.

### Q21. `Content-Length`가 없거나 잘못된 경우 어떤 문제가 생기고, 현재 코드는 어떻게 처리하나요?

답변:
body가 있는 요청에서 `Content-Length`가 잘못되면 서버가 body 끝을 잘못 판단하거나 일부만 읽을 수 있습니다. 현재 코드는 `Content-Length`가 없으면 길이를 0으로 보고 body 없는 요청처럼 처리합니다. 숫자로 파싱할 수 없는 값이면 `invalid Content-Length`로 400 응답을 반환합니다.

### Q22. request header나 body가 너무 클 때 현재 코드는 어떻게 방어하나요?

답변:
전체 request는 `HTTP_MAX_REQUEST` 65536 바이트, body는 `HTTP_MAX_BODY` 32768 바이트로 제한합니다. header 끝을 찾기 전에 전체 버퍼를 다 쓰면 `request headers too large or incomplete`로 실패합니다. body가 제한보다 크거나 header와 body 합이 전체 제한을 넘으면 400 에러로 응답해 과도한 입력을 막습니다.

### Q23. `recv()`가 한 번에 전체 request를 주지 않을 수 있는데, 현재 코드는 이를 어떻게 처리하나요?

답변:
현재 코드는 `recv()`를 반복 호출합니다. 먼저 header 끝인 `\r\n\r\n`을 찾을 때까지 반복해서 읽고, header를 파싱한 뒤 `Content-Length`만큼 body가 모두 들어올 때까지 다시 반복해서 읽습니다. 중간에 클라이언트가 연결을 닫으면 `client closed connection` 또는 `incomplete request body`로 실패 처리합니다.

### Q24. `http_extract_sql()`은 raw SQL body와 호환용 JSON body를 어떻게 구분하나요?

답변:
body 앞쪽 공백을 건너뛴 뒤 첫 문자가 `{`이면 JSON body로 보고 `"sql"` 키에서 문자열 값을 추출합니다. 그 외에는 body 전체를 raw SQL로 보고 앞뒤 공백만 제거해서 사용합니다. 그래서 기본 방식인 text/plain raw SQL과 `{"sql":"..."}` 형태를 모두 지원합니다.

### Q25. route가 없거나 method가 맞지 않을 때 각각 어떤 HTTP status를 반환하나요?

답변:
path가 `/health` 또는 `/query`인데 method가 맞지 않으면 405 Method Not Allowed를 반환합니다. 예를 들어 `GET /query`나 `POST /health`가 여기에 해당합니다. 아예 지원하지 않는 route는 404 Not Found를 반환합니다.

### Q26. “예외 처리가 잘 되어 있다”고 말하려면 현재 코드에서 구체적으로 어떤 실패 케이스들을 처리한다고 설명할 수 있나요?

답변:
HTTP 단계에서는 malformed request line, 너무 큰 header/body, 잘못된 Content-Length, body 미완성, 없는 route, 잘못된 method를 처리합니다. SQL 단계에서는 빈 SQL, 지원하지 않는 명령, 잘못된 table, 잘못된 WHERE 조건, trailing token 등을 400으로 처리합니다. DB 단계에서는 파일 열기 실패, 메모리 부족, invalid record, duplicate id, lock 획득 실패, 직렬화 실패 같은 케이스를 에러 결과로 만들고 서버가 500 응답으로 변환합니다.

## SQL 처리기와 내부 DB 엔진 연결

### Q27. SQL 문자열은 서버 내부에서 어떤 단계를 거쳐 `DbEngine` 호출로 바뀌나요?

답변:
HTTP body에서 SQL 문자열을 추출한 뒤 `sql_parse()`가 이를 `SqlStatement` 구조체로 변환합니다. `server.c`의 `execute_statement()`는 `SqlStatement`의 type, where, projection 정보를 보고 `DbFilter`와 `DbProjection`을 만듭니다. 그 결과 INSERT는 `db_insert()` 또는 `db_insert_with_id()`로, SELECT는 `db_select_projected()` 호출로 바뀝니다.

### Q28. 왜 SQL 문자열을 바로 DB engine에 넘기지 않고 `SqlStatement`로 바꾸나요?

답변:
SQL 문자열을 바로 넘기면 DB engine이 SQL 문법과 문자열 파싱까지 알아야 해서 책임이 섞입니다. `SqlStatement`로 바꾸면 SQL parser는 문법 검증과 구조화를 담당하고, DB engine은 이미 검증된 명령을 실행하는 데 집중할 수 있습니다. 또한 SELECT projection이나 WHERE 조건을 구조체 필드로 다루기 때문에 코드가 더 명확해집니다.

### Q29. `SqlStatement`, `DbFilter`, `DbResult`는 각각 어떤 역할을 하나요?

답변:
`SqlStatement`는 SQL parser가 만든 중간 표현으로, INSERT인지 SELECT인지, table, column, where 조건 등을 담습니다. `DbFilter`는 DB engine에 전달되는 검색 조건으로 전체 조회, id 조회, name 조회를 표현합니다. `DbResult`는 DB 실행 결과로 성공 여부, rows JSON, message, index 사용 여부, 실행 시간을 담는 구조체입니다.

### Q30. `execute_statement()`는 API 서버와 DB engine 사이에서 어떤 어댑터 역할을 하나요?

답변:
`execute_statement()`는 SQL 계층의 `SqlStatement`를 DB 계층의 함수 호출과 구조체로 바꾸는 변환 지점입니다. INSERT의 id 포함 여부를 보고 다른 DB 함수를 호출하고, SELECT의 WHERE와 column 정보를 `DbFilter`, `DbProjection`으로 매핑합니다. 그래서 API 서버는 SQL parser와 DB engine을 느슨하게 연결할 수 있습니다.

### Q31. DB engine이 HTTP request나 body 형식을 직접 알지 못하게 설계한 이유는 무엇인가요?

답변:
DB engine은 데이터 저장, 조회, lock, index 관리만 책임지도록 분리하기 위해서입니다. 만약 DB engine이 HTTP body나 JSON 형식까지 알면 네트워크 계층 변경이 DB 코드에 영향을 줍니다. 지금 구조에서는 HTTP API 대신 CLI나 다른 인터페이스를 붙여도 `DbEngine`은 그대로 재사용할 수 있습니다.

### Q32. 지원하지 않는 SQL이 들어오면 어떤 흐름으로 실패 응답이 만들어지나요?

답변:
`sql_parse()`가 SQL의 시작 키워드나 문법을 확인하고, 지원하지 않는 명령이면 false를 반환하면서 에러 메시지를 채웁니다. `handle_query()`는 파싱 실패를 감지해 `send_error_response(client_fd, 400, err)`를 호출합니다. 결과적으로 클라이언트는 JSON error body와 400 Bad Request를 받습니다.

### Q33. `INSERT INTO users VALUES (1, 'bumsang', 25);`와 기존 `INSERT INTO users name age VALUES 'kim' 20;`를 둘 다 지원하는 이유는 무엇인가요?

답변:
첫 번째 문법은 id를 명시해서 테스트와 benchmark에서 원하는 key를 직접 넣기 좋습니다. 두 번째 문법은 기존 학습 단계에서 사용하던 auto-increment 기반 INSERT 형식을 유지하기 위한 호환용 문법입니다. 두 방식을 모두 지원하면 이전 SQL 처리기 흐름을 살리면서도 B+ tree id 검색 시연에 필요한 데이터 구성이 쉬워집니다.

### Q34. `SELECT id, name FROM users;`처럼 컬럼 projection을 지원할 때 `SqlStatement`와 `DbProjection`은 각각 어떤 역할을 하나요?

답변:
`SqlStatement`는 SQL parser 관점에서 사용자가 어떤 column을 어떤 순서로 선택했는지 `select_columns`에 저장합니다. `execute_statement()`는 이를 DB engine 관점의 `DbProjection`으로 바꿉니다. `DbProjection`은 JSON row를 만들 때 id, name, age 중 어떤 필드를 포함할지와 출력 순서를 결정합니다.

## DB 저장소와 B+ 트리 인덱스

### Q35. `DbEngine`은 어떤 상태들을 가지고 있고, 각각 어떤 역할을 하나요?

답변:
`DbEngine`은 data file 경로, `records` 배열, 현재 row 수, 배열 capacity, 다음 auto-increment id인 `next_id`, B+ tree index, read-write lock을 가집니다. 파일 경로는 영속 저장 위치이고, `records`는 메모리에서 빠르게 조회하기 위한 row 저장소입니다. B+ tree는 id 검색을 빠르게 하기 위한 인덱스이고, lock은 여러 worker가 같은 DB 상태를 동시에 건드릴 때 일관성을 지키기 위한 장치입니다.

### Q36. 왜 실제 데이터는 `records` 배열에 두고, B+ 트리에는 `id -> record_index`만 저장하나요?

답변:
B+ tree에 전체 record를 복사해 넣으면 데이터 중복이 생기고 갱신 지점이 늘어납니다. 대신 실제 record는 배열에 한 번만 저장하고, B+ tree는 id로 배열 위치만 찾게 하면 구조가 단순해집니다. 이 방식은 인덱스가 “찾기 위한 보조 구조”라는 역할을 명확히 보여줍니다.

### Q37. 서버 시작 시 데이터 파일을 읽어 `records` 배열과 B+ 트리를 복구하는 이유는 무엇인가요?

답변:
파일은 영속 저장소이고, `records` 배열과 B+ tree는 실행 중 성능을 위한 메모리 상태입니다. 서버가 재시작되면 메모리 상태는 사라지기 때문에 파일을 다시 읽어 배열과 인덱스를 재구성해야 합니다. 이 과정을 통해 이전 INSERT 데이터가 유지되고, 시작 직후에도 id 검색 인덱스를 사용할 수 있습니다.

### Q38. INSERT 요청이 들어왔을 때 파일, 메모리 배열, B+ 트리는 각각 어떤 순서로 갱신되나요?

답변:
현재 구현은 write lock을 잡은 뒤 입력 검증과 capacity 확보를 먼저 합니다. 그 다음 data file을 append하고 닫은 뒤, `records` 배열에 record를 추가하고 `count`와 `next_id`를 갱신합니다. 마지막으로 B+ tree에 `id -> record_index`를 삽입하고 JSON 결과를 만듭니다.

### Q39. 파일 append는 성공했지만 이후 메모리 배열이나 B+ 트리 갱신이 실패하면 어떤 일관성 문제가 생길 수 있나요?

답변:
파일에는 새 record가 기록됐는데 현재 실행 중인 메모리 배열이나 B+ tree에는 반영되지 않는 불일치가 생길 수 있습니다. 특히 B+ tree 삽입이 실패하면 그 실행 중에는 id 조회로 해당 row를 찾지 못할 수 있습니다. 재시작하면 파일을 다시 읽어 복구될 수 있지만, 운영 관점에서는 rollback이나 append 순서 조정, 임시 파일/트랜잭션 같은 보완이 필요합니다.

### Q40. `BPTREE_MAX_KEYS`가 31이라는 것은 한 노드 관점에서 어떤 의미인가요?

답변:
한 B+ tree 노드가 최대 31개의 key를 가질 수 있다는 의미입니다. 리프 노드는 최대 31개의 key와 value를 저장하고, 내부 노드는 최대 31개의 separator key와 32개의 child pointer를 가집니다. 삽입으로 32개가 되는 순간 split이 발생합니다.

### Q41. 리프 노드와 내부 노드는 같은 `BpNode` 구조체를 쓰는데, 어떤 필드를 다르게 사용하나요?

답변:
리프 노드는 `keys`와 `values`를 사용해 실제 `id -> record_index` 매핑을 저장하고, `next`로 다음 리프를 연결합니다. 내부 노드는 `keys`를 separator로 사용하고, `children` 배열로 자식 노드를 가리킵니다. 같은 구조체를 쓰지만 `is_leaf` 값에 따라 의미 있는 필드가 달라집니다.

### Q42. 리프 split 후 부모로 올리는 key가 왜 오른쪽 리프의 첫 번째 key인가요?

답변:
B+ tree에서 내부 노드의 key는 실제 데이터를 담는 값이라기보다 어느 자식으로 내려갈지 결정하는 separator 역할을 합니다. 리프를 둘로 나눈 뒤 오른쪽 리프의 첫 번째 key를 부모에 올리면, 그 key 이상은 오른쪽으로 가면 된다는 기준이 됩니다. 실제 key-value는 리프에 남아 있기 때문에 검색도 리프에서 최종 확인합니다.

### Q43. 내부 노드 split에서는 왜 가운데 key를 부모로 올리고, 그 key를 자식 노드에 남기지 않나요?

답변:
내부 노드의 key는 자식 범위를 나누는 separator입니다. split 시 가운데 key를 부모로 올리면 왼쪽과 오른쪽 내부 노드의 범위를 부모가 구분할 수 있습니다. 리프 split과 달리 내부 노드의 promoted key는 데이터 record를 직접 가리키는 값이 아니므로 자식에 중복해서 남기지 않습니다.

### Q44. 루트 노드가 split될 때 트리 높이는 어떻게 증가하나요?

답변:
루트에서 split이 발생하면 기존 루트는 왼쪽 자식이 되고, 새로 생긴 오른쪽 노드는 오른쪽 자식이 됩니다. 그리고 promoted key 하나를 가진 새로운 내부 노드를 root로 만듭니다. 이 순간 트리의 높이가 1 증가합니다.

### Q45. `WHERE id = ?`는 B+ 트리를 사용하지만 `WHERE name = ?`는 왜 선형 탐색인가요?

답변:
이 프로젝트에서 만든 인덱스는 id column에 대해서만 존재하기 때문입니다. B+ tree에는 `id -> record_index`만 저장되어 있어서 id 조건은 바로 검색할 수 있습니다. name에 대한 별도 인덱스는 없으므로 `records` 배열을 처음부터 끝까지 돌면서 name이 같은지 비교해야 합니다.

### Q46. `index_used`가 true라는 설명은 정확히 어떤 조건에서만 맞나요?

답변:
현재 DB 코드 기준으로 `DbResult.index_used`는 filter type이 `DB_FILTER_ID`일 때 true가 됩니다. 즉 `SELECT ... WHERE id = N`처럼 id 조건 조회를 할 때만 B+ tree 검색 경로를 사용했다는 의미입니다. `SELECT *`, `WHERE name = ...`, INSERT는 인덱스 사용이 아니므로 false입니다.

## Thread Pool과 멀티 스레드 동시성

### Q47. 이 프로젝트에서 thread pool은 어떤 요구사항을 만족하기 위해 들어갔나요?

답변:
thread pool은 SQL 요청을 병렬로 처리하기 위해 들어갔습니다. main thread가 연결을 받고, worker thread들이 각 client fd를 처리하게 하면서 여러 요청을 동시에 수행할 수 있습니다. 또한 요청마다 thread를 새로 만들지 않고 고정 worker를 재사용해 자원 사용을 예측 가능하게 만듭니다.

### Q48. 요청이 들어올 때마다 새 thread를 만들지 않고 고정된 worker thread pool을 쓰는 이유는 무엇인가요?

답변:
thread-per-request 방식은 요청이 많아질 때 thread 생성 비용과 context switching 비용이 커지고, 동시에 생성되는 thread 수를 제어하기 어렵습니다. 고정 thread pool은 worker 수를 제한해 서버 자원 사용을 안정적으로 관리합니다. 이 프로젝트에서는 작은 규모에서도 bounded queue와 worker 재사용이라는 서버 구조를 보여주기 위해 thread pool을 사용했습니다.

### Q49. worker thread들은 어떻게 같은 queue에 접근하고, 동시에 접근해도 queue가 깨지지 않게 하나요?

답변:
worker thread들은 `ThreadPool`의 shared fd queue에서 `head` 위치의 client fd를 꺼냅니다. queue의 `head`, `tail`, `count`, `fds`는 mutex로 보호해서 한 번에 하나의 thread만 수정할 수 있게 합니다. queue가 비어 있으면 worker는 `not_empty` condition variable에서 기다립니다.

### Q50. thread pool queue에서 `head`, `tail`, `count`는 각각 어떤 역할을 하나요?

답변:
`head`는 다음에 worker가 꺼낼 fd의 위치입니다. `tail`은 main thread가 새 fd를 넣을 위치입니다. `count`는 현재 queue에 들어 있는 fd 개수로, 비었는지 가득 찼는지 판단하는 기준입니다. 세 값은 원형 큐 방식으로 함께 움직입니다.

### Q51. queue 상태를 보호하는 mutex는 정확히 어떤 race condition을 막나요?

답변:
mutex는 여러 thread가 동시에 `head`, `tail`, `count`를 수정해 queue 상태가 꼬이는 문제를 막습니다. 예를 들어 worker 두 개가 같은 `head` 값을 동시에 읽으면 같은 client fd를 중복 처리할 수 있습니다. 또 submit과 dequeue가 동시에 `count`를 바꾸면 실제 개수와 기록된 개수가 달라질 수 있는데, mutex가 이런 상황을 직렬화합니다.

### Q52. `not_empty`와 `not_full` condition variable은 각각 어떤 상황에서 사용되나요?

답변:
`not_empty`는 queue가 비어 있을 때 worker가 기다리는 조건입니다. main thread가 새 fd를 넣으면 `not_empty`를 signal해서 worker를 깨웁니다. `not_full`은 queue가 가득 찼을 때 submit 쪽이 기다리는 조건이고, worker가 fd를 하나 꺼내 공간을 만들면 signal합니다.

### Q53. `pthread_cond_wait()`를 사용할 때 왜 `if`가 아니라 `while`로 조건을 다시 확인하나요?

답변:
condition variable은 깨어났다고 해서 조건이 반드시 만족됐다는 보장이 없습니다. spurious wakeup이 있을 수 있고, 여러 thread가 동시에 깨어나면 먼저 깨어난 다른 thread가 queue 상태를 바꿔버릴 수도 있습니다. 그래서 `while`로 실제 조건을 다시 확인해야 안전합니다.

### Q54. thread pool queue의 동시성 문제와 DB engine의 동시성 문제는 어떻게 다른가요?

답변:
thread pool queue의 동시성 문제는 client fd를 넣고 빼는 작업에서 queue 자료구조가 깨지지 않게 하는 문제입니다. DB engine의 동시성 문제는 여러 요청이 같은 records 배열, 파일, next_id, B+ tree를 읽고 쓰면서 데이터 일관성이 깨지지 않게 하는 문제입니다. 전자는 producer-consumer queue 보호이고, 후자는 공유 데이터베이스 상태 보호입니다.

### Q55. DB engine에서 read-write lock을 사용한 이유는 무엇인가요?

답변:
DB 작업은 읽기와 쓰기의 성격이 다릅니다. SELECT는 DB 상태를 변경하지 않기 때문에 여러 개가 동시에 실행되어도 안전하지만, INSERT는 파일, 배열, next_id, index를 변경하므로 단독 실행이 필요합니다. read-write lock을 쓰면 여러 SELECT는 병렬로 허용하고, INSERT는 배타적으로 처리할 수 있습니다.

### Q56. SELECT는 왜 read lock을 잡고, INSERT는 왜 write lock을 잡나요?

답변:
SELECT는 records 배열과 B+ tree를 읽기만 하므로 read lock을 잡아 다른 SELECT와 동시에 실행될 수 있게 합니다. INSERT는 새 record를 만들고 파일 append, 배열 추가, next_id 갱신, B+ tree 삽입을 수행합니다. 이 작업 중 다른 thread가 읽거나 쓰면 중간 상태를 볼 수 있으므로 write lock으로 배타 접근을 보장합니다.

### Q57. `SELECT + SELECT`, `SELECT + INSERT`, `INSERT + INSERT`는 각각 동시에 실행될 수 있나요?

답변:
`SELECT + SELECT`는 둘 다 read lock이므로 동시에 실행될 수 있습니다. `SELECT + INSERT`는 INSERT가 write lock을 요구하므로 동시에 실행되지 않고 한쪽이 기다립니다. `INSERT + INSERT`도 둘 다 write lock을 요구하기 때문에 한 번에 하나만 실행됩니다.

### Q58. lock이 없다면 INSERT 중 `next_id`, `records`, 파일, B+ 트리에서 어떤 race condition이 생길 수 있나요?

답변:
lock이 없으면 두 INSERT가 같은 `next_id`를 읽어 중복 id를 만들 수 있습니다. `records` 배열의 같은 위치에 동시에 쓰거나, realloc 중 다른 thread가 기존 포인터를 읽는 문제도 생길 수 있습니다. 파일 append 순서와 B+ tree 삽입 순서가 꼬이면 파일, 메모리, 인덱스가 서로 다른 상태를 가리킬 수 있습니다.

## 발표 꼬리질문과 설계 판단

### Q59. `REQUEST_QUEUE_CAPACITY`, `SQL_MAX_LEN`, `HTTP_MAX_REQUEST`, `HTTP_MAX_BODY`, `DB_NAME_MAX`, `BPTREE_MAX_KEYS` 같은 매직 넘버의 근거를 어떻게 설명하겠나요?

답변:
이 값들은 학습용 서버에서 메모리 사용을 제한하고 입력 크기를 예측 가능하게 만들기 위한 상한값입니다. `REQUEST_QUEUE_CAPACITY` 128은 동시에 대기할 client fd 수를 제한하고, `SQL_MAX_LEN` 4096은 지원 SQL 범위에 충분한 길이를 주면서 과도한 입력을 막습니다. `HTTP_MAX_REQUEST` 65536과 `HTTP_MAX_BODY` 32768은 요청 버퍼를 제한하고, `DB_NAME_MAX` 128은 name field의 저장 크기를 정합니다. `BPTREE_MAX_KEYS` 31은 한 노드의 fan-out을 보여주면서 split 테스트가 가능하도록 둔 값입니다.

### Q60. 변수명을 `listen_fd`, `client_fd`, `records`, `next_id`, `index_used`처럼 지은 이유와, 이 프로젝트에서 가장 먼저 리팩토링하거나 테스트를 추가하고 싶은 부분은 무엇인가요?

답변:
변수명은 역할이 바로 드러나도록 지었습니다. `listen_fd`는 연결을 받는 socket, `client_fd`는 개별 클라이언트 socket, `records`는 실제 row 배열, `next_id`는 다음 auto-increment 값, `index_used`는 id 인덱스 경로 사용 여부를 의미합니다. 먼저 개선하고 싶은 부분은 서버 통합 테스트와 동시 INSERT stress test입니다. 특히 파일 append 성공 후 B+ tree 갱신 실패 같은 일관성 시나리오와 실제 HTTP 응답 형식 검증을 추가하면 프로젝트의 신뢰도가 더 높아질 것 같습니다.
