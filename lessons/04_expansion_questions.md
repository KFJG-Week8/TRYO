# 04. 사고 확장 질문: 이 프로젝트를 거점으로 CS를 넓히기

## 이 문서의 정체성
이 문서는 요약 노트가 아니라 **질문 블록 모음**입니다. 앞 문서에서 프로젝트의 큰 흐름을 이해했다면, 이제는 그 흐름을 발판 삼아 더 깊은 CS 질문으로 내려갑니다.

모든 질문은 같은 형식으로 구성됩니다.

| 항목 | 의미 |
| --- | --- |
| 질문 | 지금 붙잡을 사고의 출발점 |
| 배경 | 왜 이 질문이 중요한지 |
| 답변 | 현재 프로젝트와 일반 CS 관점에서의 설명 |
| 이 프로젝트에서 확인할 지점 | 실제 코드나 실행 결과에서 볼 수 있는 위치 |
| 더 생각할 방향 | 다음 질문으로 이어지는 길 |

정답을 외우기보다, 질문이 어떤 층을 열어 주는지 보세요.

## A. 네트워크와 TCP
### Q1. 왜 HTTP 서버는 TCP 위에서 동작할까?
**배경:** HTTP는 요청과 응답의 형식입니다. 실제 byte를 상대 프로그램까지 전달하는 일은 전송 계층이 맡습니다.

**답변:** TCP는 연결 지향 프로토콜입니다. byte 순서를 보장하고, 손실된 데이터를 재전송하며, 애플리케이션에게 순서 있는 byte stream을 제공합니다. HTTP는 그 byte stream 위에 “첫 줄은 request line”, “header와 body는 빈 줄로 구분”, “body 길이는 `Content-Length`” 같은 규칙을 얹습니다.

**이 프로젝트에서 확인할 지점:** `src/server.c`의 `socket(AF_INET, SOCK_STREAM, 0)`에서 `SOCK_STREAM`이 TCP stream socket을 뜻합니다. `src/http.c`의 `http_read_request()`는 TCP stream을 HTTP request로 해석합니다.

**더 생각할 방향:** UDP 위에 비슷한 API를 만들면 순서 보장, 재전송, 메시지 경계 문제를 누가 책임져야 할까요?

### Q2. TCP가 byte stream이라는 말은 왜 중요할까?
**배경:** 입문자는 “요청 한 번 보내면 `recv()` 한 번으로 다 읽힌다”고 생각하기 쉽습니다.

**답변:** TCP는 메시지 단위가 아니라 byte의 흐름을 제공합니다. 애플리케이션이 보낸 데이터가 수신 측에서 여러 번 나뉘어 읽힐 수도 있고, 여러 조각이 한 번에 읽힐 수도 있습니다. 그래서 HTTP처럼 상위 프로토콜이 메시지 경계를 정해야 합니다.

**이 프로젝트에서 확인할 지점:** `http_read_request()`는 header 끝인 `\r\n\r\n`을 찾을 때까지 읽고, 이후 `Content-Length`만큼 body가 올 때까지 추가로 읽습니다.

**더 생각할 방향:** HTTP/2나 WebSocket은 같은 TCP 위에서 메시지 경계를 어떻게 표현할까요?

### Q3. IP와 port는 각각 무엇을 식별할까?
**배경:** `127.0.0.1:8080` 같은 주소는 익숙하지만, IP와 port가 나누어 맡는 역할은 자주 흐려집니다.

**답변:** IP는 네트워크 안에서 host를 찾는 주소입니다. port는 그 host 안에서 특정 process 또는 service를 찾는 번호입니다. 같은 컴퓨터에서 여러 서버가 동시에 실행될 수 있는 이유는 서로 다른 port를 사용할 수 있기 때문입니다.

**이 프로젝트에서 확인할 지점:** `src/server.c`에서 `addr.sin_addr.s_addr = htonl(INADDR_ANY)`와 `addr.sin_port = htons((uint16_t)port)`가 socket을 주소와 port에 묶는 준비입니다.

**더 생각할 방향:** `127.0.0.1`, `0.0.0.0`, 실제 LAN IP는 서버 접근 범위에서 어떻게 다를까요?

### Q4. blocking I/O 서버는 언제 단순하고 언제 위험할까?
**배경:** 이 프로젝트는 blocking `accept()`, blocking `recv()`를 사용합니다. 학습에는 좋지만 실제 서비스에서는 신중해야 합니다.

**답변:** blocking I/O는 흐름이 직관적입니다. 연결이 올 때까지 기다리고, 데이터가 올 때까지 기다립니다. 하지만 느린 클라이언트가 fd를 붙잡고 있으면 worker thread가 오래 묶일 수 있습니다. 동시 연결이 많아지면 thread 수와 queue 용량이 병목이 됩니다.

**이 프로젝트에서 확인할 지점:** `server_run()`의 `accept()`와 `http_read_request()`의 `recv()`가 blocking 호출입니다.

**더 생각할 방향:** non-blocking I/O와 event loop를 쓰면 thread pool 설계는 어떻게 달라질까요?

## B. 운영체제와 File Descriptor
### Q5. file descriptor는 왜 파일과 소켓을 같은 방식으로 다루게 할까?
**배경:** C에서 파일도 fd로 다루고, socket도 fd로 다룹니다. 이것은 Unix 계열 운영체제의 중요한 추상화입니다.

**답변:** 운영체제는 열린 입출력 자원을 process 내부 fd table에 등록하고, process에게 작은 정수 번호를 줍니다. process는 그 번호로 읽기, 쓰기, 닫기 같은 연산을 요청합니다. 실제 대상이 디스크 파일인지 TCP socket인지는 kernel 내부 구현이 다릅니다.

**이 프로젝트에서 확인할 지점:** `listen_fd`, `client_fd`는 socket fd입니다. `db.c`에서 `FILE *`를 통해 다루는 data file도 내부적으로는 OS 파일 자원을 사용합니다.

**더 생각할 방향:** fd를 직접 다루는 `read/write`와 stdio의 `FILE *`는 어떤 차이가 있을까요?

### Q6. listening fd와 client fd를 구분하는 것은 왜 중요할까?
**배경:** 둘 다 정수라서 비슷해 보이지만, 역할은 완전히 다릅니다.

**답변:** listening fd는 새 TCP 연결을 기다리는 socket입니다. client fd는 `accept()`가 반환한, 특정 클라이언트와 통신하는 socket입니다. listening fd를 닫으면 서버는 새 연결을 못 받고, client fd를 닫으면 해당 클라이언트와의 연결만 끝납니다.

**이 프로젝트에서 확인할 지점:** `create_listen_socket()`은 listening fd를 만들고, `server_run()`의 accept loop는 client fd를 받아 `thread_pool_submit()`에 넘깁니다.

**더 생각할 방향:** HTTP keep-alive를 지원하면 client fd를 언제 닫아야 할까요?

### Q7. fd를 닫지 않으면 어떤 문제가 생길까?
**배경:** C 네트워크 프로그래밍에서 resource cleanup은 기능만큼 중요합니다.

**답변:** fd를 닫지 않으면 process의 fd table에 열린 자원이 계속 남습니다. 요청이 많아지면 열 수 있는 fd 한도에 도달해 새 연결이나 파일 open이 실패할 수 있습니다. 네트워크 연결도 상대방 입장에서 계속 열린 것처럼 보일 수 있습니다.

**이 프로젝트에서 확인할 지점:** `handle_client()` 마지막의 `close(client_fd)`가 요청 처리 후 client fd를 정리합니다.

**더 생각할 방향:** 에러가 중간에 발생해도 fd가 반드시 닫히도록 하려면 C 코드 구조를 어떻게 잡아야 할까요?

## C. HTTP와 API Boundary
### Q8. `Content-Length`를 믿는 parser는 왜 조심해야 할까?
**배경:** HTTP body 길이를 알기 위해 `Content-Length`를 사용하지만, 클라이언트가 항상 정직하거나 정상이라는 보장은 없습니다.

**답변:** `Content-Length`가 실제 body보다 크면 서버는 더 읽으려고 기다릴 수 있습니다. 너무 큰 값이면 메모리 사용 문제가 생깁니다. 실제 서버는 최대 body 크기, timeout, header validation, connection policy를 함께 둡니다.

**이 프로젝트에서 확인할 지점:** `HTTP_MAX_BODY`, `HTTP_MAX_REQUEST` 제한과 `request body too large` 에러 경로가 있습니다.

**더 생각할 방향:** slowloris 같은 공격은 HTTP header/body 읽기 정책을 어떻게 악용할까요?

### Q9. 왜 이 프로젝트는 `Connection: close`를 사용할까?
**배경:** HTTP/1.1에서는 연결을 재사용하는 keep-alive가 흔합니다. 하지만 구현 난이도는 올라갑니다.

**답변:** `Connection: close`는 응답 하나를 보낸 뒤 client fd를 닫겠다는 뜻입니다. 이렇게 하면 한 fd에서 여러 요청을 연속 처리할 필요가 없어 parser와 lifecycle이 단순해집니다.

**이 프로젝트에서 확인할 지점:** `http_send_json()`이 `Connection: close` header를 응답에 넣고, `handle_client()`는 마지막에 `close(client_fd)`를 호출합니다.

**더 생각할 방향:** keep-alive를 지원하려면 `handle_client()`는 어떤 loop 구조로 바뀌어야 할까요?

### Q10. JSON body에 SQL을 담는 API는 좋은 설계일까?
**배경:** 학습 프로젝트에서는 SQL을 직접 보내는 방식이 직관적입니다. 하지만 실제 서비스에서는 위험할 수 있습니다.

**답변:** 내부 DBMS API나 학습용 도구라면 SQL 전달이 단순합니다. 하지만 외부 사용자에게 SQL 실행 API를 공개하면 권한, 데이터 노출, injection, 리소스 남용 문제가 생깁니다. 실제 서비스는 보통 제한된 endpoint와 parameter를 제공합니다.

**이 프로젝트에서 확인할 지점:** `http_extract_sql()`은 JSON에서 SQL 문자열을 꺼내고, `sql_parse()`는 지원 문법만 허용합니다.

**더 생각할 방향:** SQL 대신 `{"operation":"get_user","id":1}` 같은 API를 만들면 서버 구조는 어떻게 달라질까요?

## D. Thread Pool과 Queue
### Q11. 요청마다 thread를 새로 만들지 않는 이유는 무엇일까?
**배경:** thread는 싸지 않습니다. 생성, 스케줄링, stack 메모리 모두 비용이 있습니다.

**답변:** thread pool은 worker를 미리 만들어 재사용합니다. 요청이 들어오면 fd를 queue에 넣고, worker가 꺼내 처리합니다. 이렇게 하면 thread 생성 비용을 줄이고 동시에 처리할 작업 수를 제한할 수 있습니다.

**이 프로젝트에서 확인할 지점:** `thread_pool_init()`이 worker들을 만들고, `thread_pool_submit()`이 fd를 queue에 넣습니다.

**더 생각할 방향:** CPU-bound 작업과 I/O-bound 작업에서 적절한 worker 수는 왜 달라질까요?

### Q12. queue가 가득 찬다는 것은 시스템적으로 무슨 뜻일까?
**배경:** bounded queue는 단순한 배열이 아니라 서버의 처리 용량을 드러내는 장치입니다.

**답변:** queue가 가득 찼다는 것은 요청이 들어오는 속도가 처리 속도보다 빠르다는 뜻입니다. 이때 기다리기, 거절하기, timeout 처리하기, upstream에 backpressure 주기 같은 정책이 필요합니다.

**이 프로젝트에서 확인할 지점:** `thread_pool_submit()`은 `pool->count == pool->capacity`이면 `not_full` condition variable에서 기다립니다.

**더 생각할 방향:** 실제 API 서버라면 queue full 상황에서 503 응답을 보내는 편이 나을까요, 기다리는 편이 나을까요?

### Q13. condition variable은 왜 mutex와 함께 쓰일까?
**배경:** condition variable은 혼자서 공유 상태를 보호하지 않습니다.

**답변:** condition variable은 “조건이 바뀌었다”는 신호를 주는 장치이고, mutex는 그 조건을 표현하는 공유 상태를 보호합니다. queue가 비었는지, 가득 찼는지 확인하는 동안 다른 thread가 상태를 바꾸면 안 됩니다.

**이 프로젝트에서 확인할 지점:** `worker_loop()`와 `thread_pool_submit()` 모두 condition wait 전후에 mutex를 잡습니다.

**더 생각할 방향:** condition wait를 `if`가 아니라 `while`로 감싸는 이유는 무엇일까요?

## E. 동시성 제어와 Lock
### Q14. SELECT는 왜 read lock만 잡아도 될까?
**배경:** lock은 무조건 하나만 실행하게 만드는 도구가 아닙니다. 읽기와 쓰기의 성격이 다르면 다르게 다룰 수 있습니다.

**답변:** SELECT는 DB 상태를 바꾸지 않습니다. 여러 SELECT가 동시에 record 배열을 읽어도 데이터가 깨지지 않습니다. read-write lock은 여러 read lock을 동시에 허용하므로 읽기 병렬성을 얻을 수 있습니다.

**이 프로젝트에서 확인할 지점:** `db_select()`는 `pthread_rwlock_rdlock()`을 호출합니다.

**더 생각할 방향:** 긴 SELECT가 계속 실행되면 INSERT는 얼마나 기다릴 수 있을까요?

### Q15. INSERT는 왜 write lock이 필요할까?
**배경:** INSERT는 공유 상태 여러 개를 순서대로 바꿉니다.

**답변:** INSERT는 `next_id`, CSV 파일, `records` 배열, B+ tree index를 모두 갱신합니다. 이 작업 중간에 SELECT가 들어오면 파일에는 있는데 index에는 없거나, 배열에는 있는데 count가 맞지 않는 상태를 볼 수 있습니다. write lock은 이 구간을 하나의 안전한 critical section으로 만듭니다.

**이 프로젝트에서 확인할 지점:** `db_insert()`는 시작 부분에서 `pthread_rwlock_wrlock()`을 잡고, 성공/실패 경로에서 unlock합니다.

**더 생각할 방향:** UPDATE와 DELETE가 추가되면 어떤 상태들을 추가로 보호해야 할까요?

### Q16. race condition은 꼭 충돌이 눈에 보여야만 문제일까?
**배경:** 동시성 버그는 재현이 어렵습니다. 테스트에서 한 번 통과했다고 안전하다고 볼 수 없습니다.

**답변:** race condition은 실행 순서에 따라 결과가 달라지는 문제입니다. 어떤 순서에서는 정상처럼 보이고, 어떤 순서에서는 데이터가 깨집니다. 그래서 공유 상태에 접근하는 규칙을 코드 구조로 강제해야 합니다.

**이 프로젝트에서 확인할 지점:** `records`, `count`, `next_id`, `index`는 모두 `DbEngine` 안의 공유 상태이며 lock 아래에서 접근됩니다.

**더 생각할 방향:** lock을 너무 크게 잡으면 안전하지만 성능은 어떻게 될까요?

## F. SQL Parser와 실행 경계
### Q17. SQL parser는 왜 DB engine과 분리되어야 할까?
**배경:** 문자열 해석과 데이터 실행은 서로 다른 문제입니다.

**답변:** parser는 SQL 문법을 검사하고 의미를 구조화합니다. DB engine은 구조화된 의도에 따라 파일과 메모리 상태를 바꿉니다. 둘을 분리하면 문법 오류와 실행 오류를 구분할 수 있고, 문법 확장도 더 명확해집니다.

**이 프로젝트에서 확인할 지점:** `sql_parse()`는 `SqlStatement`를 만들고, `execute_statement()`가 이를 `db_insert()` 또는 `db_select()` 호출로 연결합니다.

**더 생각할 방향:** `ORDER BY`를 추가하면 parser와 DB engine 중 어디가 얼마나 바뀌어야 할까요?

### Q18. 문자열을 구조체로 바꾸는 것은 왜 중요한 추상화일까?
**배경:** 문자열은 사람이 읽기 좋지만 프로그램이 안정적으로 처리하기에는 모호합니다.

**답변:** 구조체는 의미를 필드로 분리합니다. `type`, `where_type`, `where_id` 같은 필드를 보면 다음 단계는 문자열 검색 없이 실행 결정을 내릴 수 있습니다. 이는 작은 형태의 AST 또는 execution intent입니다.

**이 프로젝트에서 확인할 지점:** `include/sql.h`의 `SqlStatement` 구조체를 보면 지원 SQL 범위가 그대로 드러납니다.

**더 생각할 방향:** 복잡한 WHERE 조건을 표현하려면 단일 구조체보다 tree 형태가 필요한 이유는 무엇일까요?

### Q19. parser가 지원하지 않는 문법을 거부하는 것은 왜 기능일까?
**배경:** “더 많이 허용하는 것”이 항상 좋은 것은 아닙니다.

**답변:** 지원하지 않는 문법을 애매하게 처리하면 잘못된 실행이나 데이터 손상이 생길 수 있습니다. parser가 명확히 거부하면 시스템의 계약이 분명해집니다. 최소구현에서는 특히 “어디까지 되는지”를 정확히 알려주는 것이 중요합니다.

**이 프로젝트에서 확인할 지점:** `sql_parse()`와 helper parser들은 실패 시 error message를 채우고 0을 반환합니다.

**더 생각할 방향:** 사용자에게 문법 오류 위치까지 알려주려면 parser는 어떤 정보를 더 보관해야 할까요?

## G. Storage와 Persistence
### Q20. 파일에 append하는 저장 방식은 왜 단순하고 강력할까?
**배경:** DB 저장소를 처음 구현할 때 가장 이해하기 쉬운 방식은 record를 한 줄씩 추가하는 것입니다.

**답변:** append는 기존 데이터를 덮어쓰지 않고 새 데이터를 끝에 추가합니다. 구현이 쉽고, INSERT 흐름을 이해하기 좋습니다. 하지만 UPDATE, DELETE, compaction, crash recovery 같은 문제는 아직 해결하지 못합니다.

**이 프로젝트에서 확인할 지점:** `db_insert()`는 `fprintf(file, "%d,%s,%d\n", ...)`로 CSV line을 append합니다.

**더 생각할 방향:** DELETE를 지원하면 파일에서 기존 line을 어떻게 처리해야 할까요?

### Q21. 서버 시작 시 파일을 읽어 인덱스를 다시 만드는 방식의 장단점은 무엇일까?
**배경:** 이 프로젝트는 B+ tree index를 파일에 저장하지 않습니다.

**답변:** 장점은 구현이 단순하다는 것입니다. CSV 파일만 있으면 서버 시작 때 records 배열과 B+ tree를 복구할 수 있습니다. 단점은 데이터가 커질수록 startup 시간이 길어지고, index 자체를 영속화하지 않으므로 매번 재구축 비용이 듭니다.

**이 프로젝트에서 확인할 지점:** `db_init()`은 파일을 읽으며 `load_record()`를 호출하고, `load_record()`는 `bptree_insert()`를 호출합니다.

**더 생각할 방향:** index를 디스크에 저장하려면 node pointer 대신 무엇을 저장해야 할까요?

### Q22. 파일 write 성공 후 index update 실패는 어떤 문제를 만들까?
**배경:** 여러 상태를 순서대로 갱신하는 시스템은 중간 실패를 고민해야 합니다.

**답변:** 파일에는 record가 들어갔지만 메모리 배열이나 index 갱신이 실패하면, 현재 서버 실행 중 상태와 디스크 상태가 어긋날 수 있습니다. 이 프로젝트는 학습용이라 복구 전략이 단순하지만, 실제 DBMS는 write-ahead log와 transaction으로 이런 문제를 다룹니다.

**이 프로젝트에서 확인할 지점:** `db_insert()`는 파일 append 후 records 배열과 B+ tree를 갱신합니다.

**더 생각할 방향:** INSERT 전체를 atomic하게 만들려면 어떤 로그 또는 rollback 전략이 필요할까요?

### Q23. CSV 저장 포맷은 왜 학습에는 좋지만 실제 DB에는 부족할까?
**배경:** CSV는 사람이 읽기 쉽지만 DB page format으로는 한계가 많습니다.

**답변:** CSV는 parsing이 쉽고 디버깅이 편합니다. 하지만 문자열 escaping, type validation, 빠른 random access, page 단위 I/O, 압축, schema 변경에는 약합니다. 실제 DBMS는 binary page format을 사용하는 경우가 많습니다.

**이 프로젝트에서 확인할 지점:** `db_init()`의 `sscanf(line, "%d,%127[^,],%d", ...)`가 CSV line을 record로 읽습니다.

**더 생각할 방향:** name에 comma가 들어가면 현재 저장 포맷은 어떻게 깨질까요?

## H. B+ Tree와 Index
### Q24. index는 왜 검색을 빠르게 하지만 INSERT 비용을 늘릴까?
**배경:** 인덱스는 공짜가 아닙니다. 읽기 성능과 쓰기 비용 사이의 tradeoff입니다.

**답변:** SELECT는 index 덕분에 전체 record를 보지 않고 원하는 위치를 찾을 수 있습니다. 하지만 INSERT 때는 record만 추가하는 것이 아니라 index에도 새 key를 등록해야 합니다. node split이 발생하면 더 많은 작업이 필요합니다.

**이 프로젝트에서 확인할 지점:** `db_insert()`는 record 추가 후 `bptree_insert()`를 호출합니다.

**더 생각할 방향:** name에도 index를 추가하면 SELECT와 INSERT의 비용은 각각 어떻게 변할까요?

### Q25. B+ tree는 왜 leaf node가 중요할까?
**배경:** B+ tree에서 실제 value는 보통 leaf에 모입니다.

**답변:** internal node는 검색 방향을 결정하고, leaf node는 실제 key/value를 담습니다. leaf들이 연결되어 있으면 range scan도 효율적으로 할 수 있습니다. 이 프로젝트는 range query를 구현하지 않았지만, `next` pointer가 leaf 연결 가능성을 보여 줍니다.

**이 프로젝트에서 확인할 지점:** `BpNode`에는 `values`와 `next`가 있고, `leaf_insert()`가 split 시 `right->next = leaf->next; leaf->next = right;`를 수행합니다.

**더 생각할 방향:** `SELECT * FROM users WHERE id BETWEEN 10 AND 20;`을 구현한다면 `next` pointer를 어떻게 활용할 수 있을까요?

### Q26. promoted key는 왜 필요한가?
**배경:** node가 split되면 부모가 두 child를 구분할 기준을 알아야 합니다.

**답변:** promoted key는 “오른쪽 node에는 이 key 이상이 있다”는 경계 정보입니다. 부모는 이 key를 기준으로 검색 방향을 선택합니다. split 정보가 아래에서 위로 올라가기 때문에 tree 전체가 균형을 유지할 수 있습니다.

**이 프로젝트에서 확인할 지점:** `leaf_insert()`와 `node_insert()`는 `InsertResult`에 `promoted_key`와 `right` node를 담아 반환합니다.

**더 생각할 방향:** root까지 split되면 왜 새 root가 필요할까요?

### Q27. 이 프로젝트의 B+ tree는 실제 DBMS 인덱스와 무엇이 다를까?
**배경:** 이름은 B+ tree지만 구현 환경이 다르면 고려할 문제가 달라집니다.

**답변:** 이 프로젝트의 B+ tree는 메모리 pointer로 node를 연결합니다. 실제 DBMS는 보통 disk page id나 offset으로 node를 찾습니다. 또한 실제 DBMS index는 crash recovery, concurrency, page split logging, range scan, composite key와 연결됩니다.

**이 프로젝트에서 확인할 지점:** `BpNode *children[]`는 메모리 pointer입니다. data file에는 B+ tree node가 저장되지 않습니다.

**더 생각할 방향:** node를 디스크 page로 저장하려면 pointer 대신 어떤 식별자가 필요할까요?

## I. 최소구현의 한계와 가치
### Q28. 최소구현은 왜 학습에 강할까?
**배경:** 기능이 적으면 초라해 보일 수 있지만, 학습에서는 핵심 구조가 잘 보이는 장점이 있습니다.

**답변:** 라이브러리와 프레임워크가 많은 일을 대신하면 입문자는 경계를 보기 어렵습니다. 이 프로젝트는 HTTP parsing, SQL parsing, DB 실행, index lookup이 코드에 드러납니다. 그래서 “어디서 무엇이 바뀌는지”를 추적하기 좋습니다.

**이 프로젝트에서 확인할 지점:** `src/http.c`, `src/sql.c`, `src/db.c`, `src/bptree.c`가 각각 한 계층을 직접 보여 줍니다.

**더 생각할 방향:** 학습 프로젝트에서 직접 구현할 것과 라이브러리를 쓸 것을 나누는 기준은 무엇이어야 할까요?

### Q29. 이 프로젝트의 HTTP parser 한계는 어떤 발전 방향을 열까?
**배경:** 최소 parser는 의도적으로 작지만, 실제 HTTP traffic은 다양합니다.

**답변:** 현재 parser는 request line, `Content-Length`, body 중심입니다. 실제 서버로 발전하려면 timeout, keep-alive, chunked transfer, header parsing 강화, 잘못된 요청 처리, 큰 body 정책이 필요합니다.

**이 프로젝트에서 확인할 지점:** `http_read_request()`가 `strstr(buffer, "\r\n\r\n")`와 `parse_content_length()`에 크게 의존합니다.

**더 생각할 방향:** 직접 HTTP parser를 계속 키우는 것과 검증된 HTTP library를 쓰는 것 사이의 tradeoff는 무엇일까요?

### Q30. 이 프로젝트의 DB는 transaction을 지원하지 않는데, 왜 이것이 중요한 한계일까?
**배경:** DBMS의 핵심은 단순 저장이 아니라 일관성과 복구입니다.

**답변:** transaction은 여러 작업을 하나의 논리적 단위로 묶고, 성공하면 모두 반영하고 실패하면 모두 되돌리는 개념입니다. 현재 프로젝트는 INSERT 중간 실패나 crash 상황을 정교하게 복구하지 않습니다. 그래서 학습용 DB engine이지 production DBMS는 아닙니다.

**이 프로젝트에서 확인할 지점:** `db_insert()`는 파일 append, records 배열, B+ tree 갱신을 순서대로 수행하지만 write-ahead log나 rollback은 없습니다.

**더 생각할 방향:** WAL을 추가한다면 어떤 정보를 먼저 기록해야 할까요?

## J. 실제 시스템으로 확장하기
### Q31. 이 서버를 실제 API 서비스로 키우려면 무엇을 먼저 바꿔야 할까?
**배경:** production 시스템은 기능뿐 아니라 안정성, 보안, 운영 가능성을 요구합니다.

**답변:** 우선 검증된 HTTP/JSON parser, timeout, request size limit, 인증/권한, structured logging, metrics가 필요합니다. DB 쪽은 transaction, recovery, schema 관리, index persistence가 필요합니다.

**이 프로젝트에서 확인할 지점:** README의 curl API는 학습용으로 열려 있으며 인증이 없습니다. `http_extract_sql()`도 최소 JSON parser입니다.

**더 생각할 방향:** 가장 먼저 강화해야 할 것이 보안인지 안정성인지 성능인지는 서비스 목표에 따라 어떻게 달라질까요?

### Q32. DB engine을 HTTP 밖에서도 재사용하려면 무엇이 달라져야 할까?
**배경:** 좋은 모듈은 특정 입출력 방식에 지나치게 묶이지 않습니다.

**답변:** DB engine은 HTTP를 몰라야 CLI, 테스트, 다른 server protocol에서도 재사용할 수 있습니다. 현재도 DB 함수는 socket을 직접 모르지만, `DbResult`가 JSON 문자열을 담는 부분은 표현 계층과 조금 섞여 있습니다.

**이 프로젝트에서 확인할 지점:** `db_select()`는 `rows_json`을 직접 만듭니다. 이 설계는 단순하지만, response format을 바꾸려면 DB 쪽도 영향을 받습니다.

**더 생각할 방향:** `DbResult`가 `Record` 배열을 돌려주고 JSON 직렬화는 server layer에서 한다면 장단점은 무엇일까요?

### Q33. 관측 가능성은 왜 기능 구현만큼 중요할까?
**배경:** 시스템은 내부에서 무슨 일이 일어나는지 볼 수 있어야 운영하고 디버깅할 수 있습니다.

**답변:** 로그, metrics, tracing은 실행 중인 시스템을 이해하게 해 줍니다. 이 프로젝트의 `thread`, `fd`, `elapsed_us`, `index_used`는 작은 관측 가능성 장치입니다. 실제 시스템에서는 request id, error rate, latency percentile, slow query log가 필요합니다.

**이 프로젝트에서 확인할 지점:** `log_request()`는 요청 처리 결과를 한 줄 로그로 남깁니다.

**더 생각할 방향:** 여러 서버 인스턴스가 있을 때 request 하나를 끝까지 추적하려면 어떤 id가 필요할까요?

## 마지막 정리
이 프로젝트는 작지만 여러 CS 층이 겹쳐 있습니다.

```text
network
  -> OS resource
  -> protocol parsing
  -> concurrent execution
  -> query parsing
  -> storage
  -> indexing
  -> observability
```

Top-Down 학습의 핵심은 질문을 멈추지 않는 것입니다. 하나의 질문이 다음 계층을 열고, 그 계층에서 다시 더 구체적인 질문이 나옵니다. 이 문서를 다시 읽을 때는 답변보다 질문의 방향을 더 오래 붙잡아 보세요.
