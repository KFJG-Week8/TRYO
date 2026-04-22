# WEEK8 미니 DBMS API 서버 면접 대비 질문 60개

## API 서버 아키텍처

1. 이 프로젝트에서 API 서버는 정확히 어떤 역할을 하나요?
2. `main.c`, `server.c`, `http.c`, `sql.c`, `db.c`, `bptree.c`는 각각 어떤 책임을 갖나요?
3. 클라이언트가 `POST /query`를 보냈을 때 서버 내부 흐름을 처음부터 끝까지 설명해보세요.
4. `socket() -> bind() -> listen() -> accept()` 흐름에서 각 단계는 어떤 역할을 하나요?
5. `listen_fd`와 `client_fd`의 차이는 무엇인가요?
6. 왜 main thread가 요청을 직접 처리하지 않고 worker thread에게 넘기나요?
7. `GET /health`와 `POST /query`를 분리한 이유는 무엇인가요?
8. 이 서버가 HTTP keep-alive를 지원하지 않고 요청마다 연결을 닫는 이유는 무엇인가요?
9. 서버 로그에는 `127.0.0.1`로 출력되지만 실제 bind는 `INADDR_ANY`입니다. 이 차이는 무엇을 의미하나요?
10. 다른 팀이 CLI 명령으로만 DBMS를 조작했다면, 이 프로젝트가 HTTP API 서버 구조를 택한 장단점은 무엇인가요?

## 외부 클라이언트와 CLI 환경

11. “외부 클라이언트에서 DBMS 기능을 사용할 수 있어야 한다”는 요구사항을 이 코드는 어떻게 만족하나요?
12. 별도의 `client.c`가 없어도 `curl`을 외부 클라이언트로 볼 수 있나요?
13. 외부 PC에서 이 서버에 접속하려면 코드 외에 어떤 네트워크 조건이 필요하나요?
14. `./bin/week8_dbms [port] [thread_count] [data_file]` 형태의 CLI는 어떤 점에서 시연과 테스트에 유리한가요?
15. port, thread_count, data_file을 실행 인자로 받을 수 있게 한 이유는 무엇인가요?
16. 잘못된 port나 thread_count가 들어왔을 때 fallback 값을 쓰는 설계는 어떤 장단점이 있나요?
17. port 기본값을 8080, worker thread 기본값을 4, data file 기본값을 `data/users.csv`로 둔 근거를 어떻게 설명하겠나요?
18. CLI 환경에서 `make`, 서버 실행, `curl`, `scripts/benchmark.sh`는 각각 어떤 역할을 하나요?

## HTTP 처리와 예외 처리

19. 이 서버는 HTTP request의 끝을 어떻게 판단하나요?
20. TCP byte stream과 HTTP request message의 차이는 무엇인가요?
21. `Content-Length`가 없거나 잘못된 경우 어떤 문제가 생기고, 현재 코드는 어떻게 처리하나요?
22. request header나 body가 너무 클 때 현재 코드는 어떻게 방어하나요?
23. `recv()`가 한 번에 전체 request를 주지 않을 수 있는데, 현재 코드는 이를 어떻게 처리하나요?
24. `http_extract_sql()`은 완전한 JSON parser가 아닌데, 어떤 입력 범위까지만 처리하나요?
25. route가 없거나 method가 맞지 않을 때 각각 어떤 HTTP status를 반환하나요?
26. “예외 처리가 잘 되어 있다”고 말하려면 현재 코드에서 구체적으로 어떤 실패 케이스들을 처리한다고 설명할 수 있나요?

## SQL 처리기와 내부 DB 엔진 연결

27. SQL 문자열은 서버 내부에서 어떤 단계를 거쳐 `DbEngine` 호출로 바뀌나요?
28. 왜 SQL 문자열을 바로 DB engine에 넘기지 않고 `SqlStatement`로 바꾸나요?
29. `SqlStatement`, `DbFilter`, `DbResult`는 각각 어떤 역할을 하나요?
30. `execute_statement()`는 API 서버와 DB engine 사이에서 어떤 어댑터 역할을 하나요?
31. DB engine이 HTTP request나 JSON body를 직접 알지 못하게 설계한 이유는 무엇인가요?
32. 지원하지 않는 SQL이 들어오면 어떤 흐름으로 실패 응답이 만들어지나요?
33. `INSERT INTO users name age VALUES 'kim' 20;`처럼 제한된 SQL 문법만 지원하는 이유를 어떻게 설명하겠나요?
34. 다른 팀이 SQL parser 없이 JSON 필드로 직접 insert/select를 구현했다면, 이 프로젝트의 SQL parser 기반 설계는 어떤 장단점이 있나요?

## DB 저장소와 B+ 트리 인덱스

35. `DbEngine`은 어떤 상태들을 가지고 있고, 각각 어떤 역할을 하나요?
36. 왜 실제 데이터는 `records` 배열에 두고, B+ 트리에는 `id -> record_index`만 저장하나요?
37. 서버 시작 시 데이터 파일을 읽어 `records` 배열과 B+ 트리를 복구하는 이유는 무엇인가요?
38. INSERT 요청이 들어왔을 때 파일, 메모리 배열, B+ 트리는 각각 어떤 순서로 갱신되나요?
39. 파일 append는 성공했지만 이후 메모리 배열이나 B+ 트리 갱신이 실패하면 어떤 일관성 문제가 생길 수 있나요?
40. `BPTREE_MAX_KEYS`가 31이라는 것은 한 노드 관점에서 어떤 의미인가요?
41. 리프 노드와 내부 노드는 같은 `BpNode` 구조체를 쓰는데, 어떤 필드를 다르게 사용하나요?
42. 리프 split 후 부모로 올리는 key가 왜 오른쪽 리프의 첫 번째 key인가요?
43. 내부 노드 split에서는 왜 가운데 key를 부모로 올리고, 그 key를 자식 노드에 남기지 않나요?
44. 루트 노드가 split될 때 트리 높이는 어떻게 증가하나요?
45. `WHERE id = ?`는 B+ 트리를 사용하지만 `WHERE name = ?`는 왜 선형 탐색인가요?
46. `index_used`가 true라는 설명은 정확히 어떤 조건에서만 맞나요?

## Thread Pool과 멀티 스레드 동시성

47. 이 프로젝트에서 thread pool은 어떤 요구사항을 만족하기 위해 들어갔나요?
48. 요청이 들어올 때마다 새 thread를 만들지 않고 고정된 worker thread pool을 쓰는 이유는 무엇인가요?
49. worker thread들은 어떻게 같은 queue에 접근하고, 동시에 접근해도 queue가 깨지지 않게 하나요?
50. thread pool queue에서 `head`, `tail`, `count`는 각각 어떤 역할을 하나요?
51. queue 상태를 보호하는 mutex는 정확히 어떤 race condition을 막나요?
52. `not_empty`와 `not_full` condition variable은 각각 어떤 상황에서 사용되나요?
53. `pthread_cond_wait()`를 사용할 때 왜 `if`가 아니라 `while`로 조건을 다시 확인하나요?
54. thread pool queue의 동시성 문제와 DB engine의 동시성 문제는 어떻게 다른가요?
55. DB engine에서 read-write lock을 사용한 이유는 무엇인가요?
56. SELECT는 왜 read lock을 잡고, INSERT는 왜 write lock을 잡나요?
57. `SELECT + SELECT`, `SELECT + INSERT`, `INSERT + INSERT`는 각각 동시에 실행될 수 있나요?
58. lock이 없다면 INSERT 중 `next_id`, `records`, 파일, B+ 트리에서 어떤 race condition이 생길 수 있나요?

## 발표 꼬리질문과 설계 판단

59. `REQUEST_QUEUE_CAPACITY`, `SQL_MAX_LEN`, `HTTP_MAX_REQUEST`, `HTTP_MAX_BODY`, `DB_NAME_MAX`, `BPTREE_MAX_KEYS` 같은 매직 넘버의 근거를 어떻게 설명하겠나요?
60. 변수명을 `listen_fd`, `client_fd`, `records`, `next_id`, `index_used`처럼 지은 이유와, 이 프로젝트에서 가장 먼저 리팩토링하거나 테스트를 추가하고 싶은 부분은 무엇인가요?
