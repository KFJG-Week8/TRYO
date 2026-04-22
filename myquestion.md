# 내 질문과 답변

## Q1. mutex는 어떻게 한 번에 하나만 접근한다는 사실을 보장할 수 있나?

짧은 답변:

```text
mutex는 운영체제와 pthread 라이브러리가 제공하는 동기화 객체입니다.
한 thread가 pthread_mutex_lock()에 성공해 mutex를 소유하면,
다른 thread는 같은 mutex에 대해 pthread_mutex_lock()을 호출해도 unlock될 때까지 기다립니다.
그래서 mutex로 감싼 구간에는 한 번에 하나의 thread만 들어갈 수 있습니다.
```

이 코드에서는 `src/thread_pool.c`의 queue 접근 구간이 mutex로 보호됩니다.

```c
pthread_mutex_lock(&pool->mutex); // fd queue를 읽고 쓰기 전에 mutex를 잠급니다.

client_fd = pool->fds[pool->head]; // queue의 head 위치에 있는 client fd를 꺼냅니다.
pool->head = (pool->head + 1) % pool->capacity; // 원형 queue라서 head를 다음 칸으로 이동합니다.
pool->count--; // queue 안에 남은 fd 개수를 1 줄입니다.

pthread_mutex_unlock(&pool->mutex); // queue 수정이 끝났으므로 다른 thread가 들어올 수 있게 풉니다.
```

여기서 보호하려는 공유 데이터는 다음입니다.

```text
pool->fds
pool->head
pool->tail
pool->count
pool->stopping
```

조금 더 자세히:

```text
mutex는 "약속"이 아니라 실제로 thread를 막는 장치입니다.
이미 잠긴 mutex에 다른 thread가 lock을 시도하면,
그 thread는 그 자리에서 진행하지 못하고 대기 상태가 됩니다.

먼저 들어간 thread가 pthread_mutex_unlock()을 호출해야
대기하던 thread 중 하나가 다시 lock을 얻고 다음 줄로 진행할 수 있습니다.
```

주의할 점:

```text
mutex 자체가 모든 변수를 자동으로 보호하는 것은 아닙니다.
공유 데이터를 읽고 쓰는 모든 코드가 같은 mutex를 사용해야 보호됩니다.

이 코드에서는 fd queue를 만지는 thread_pool_submit()과 worker_loop()가
모두 pool->mutex를 사용하기 때문에 queue 상태가 보호됩니다.
```

발표용 한 문장:

```text
mutex는 같은 공유 자원에 들어가는 입구를 하나로 만들고,
먼저 들어간 thread가 나올 때까지 다른 thread를 세워두는 장치입니다.
```

## Q2. worker thread는 queue에 막 접근한다고 했는데, 그게 어떻게 가능한가?

짧은 답변:

```text
worker thread들은 모두 같은 ThreadPool*을 인자로 받습니다.
ThreadPool 안에는 fd queue와 mutex, condition variable이 들어 있습니다.
그래서 모든 worker가 같은 queue를 볼 수 있습니다.
다만 동시에 마음대로 접근하는 것이 아니라, queue를 만질 때마다 mutex를 잡고 접근합니다.
```

worker thread 생성 부분:

```c
pthread_create(&pool->threads[i], NULL, worker_loop, pool) // i번째 worker thread를 만들고 같은 ThreadPool* pool을 넘깁니다.
```

여기서 마지막 인자인 `pool`이 모든 worker에게 전달됩니다.

worker는 이렇게 시작합니다.

```c
static void *worker_loop(void *arg) // pthread_create가 새 thread에서 실행할 함수입니다.
{ // worker thread의 함수 본문이 시작됩니다.
    ThreadPool *pool = arg; // void*로 넘어온 인자를 다시 ThreadPool*로 해석합니다.
    ... // 실제 코드에서는 이 pool에서 client fd를 꺼내 처리합니다.
} // worker_loop 함수가 끝나면 해당 worker thread도 종료됩니다.
```

즉 구조는 이렇습니다.

```text
ThreadPool pool
  |
  +--> worker thread 1이 같은 pool 주소를 받음
  +--> worker thread 2가 같은 pool 주소를 받음
  +--> worker thread 3이 같은 pool 주소를 받음
  +--> worker thread 4가 같은 pool 주소를 받음
```

그래서 worker들은 같은 queue에 접근할 수 있습니다.

```c
client_fd = pool->fds[pool->head]; // queue의 맨 앞(head)에 있는 client fd를 가져옵니다.
pool->head = (pool->head + 1) % pool->capacity; // head를 다음 위치로 옮기고 끝이면 0으로 돌아갑니다.
pool->count--; // fd 하나를 꺼냈으므로 queue에 남은 개수를 줄입니다.
```

하지만 이 코드는 항상 mutex 안에서 실행됩니다.

```c
pthread_mutex_lock(&pool->mutex); // queue 공유 상태에 들어가기 전에 lock을 잡습니다.
... // 이 사이에서 head, tail, count, fds 같은 공유 값을 읽거나 바꿉니다.
pthread_mutex_unlock(&pool->mutex); // 공유 상태 수정이 끝나면 lock을 풉니다.
```

정리:

```text
접근이 가능한 이유
  모든 worker가 같은 ThreadPool* 주소를 공유하기 때문입니다.

동시에 깨지지 않는 이유
  queue 상태를 읽고 쓰는 순간에는 pool->mutex를 잡기 때문입니다.
```

발표용 한 문장:

```text
worker thread들은 같은 ThreadPool 구조체 주소를 공유하므로 queue를 볼 수 있고,
queue를 실제로 수정할 때는 mutex를 잡아서 한 번에 하나씩만 수정합니다.
```

## Q3. condition variable은 어떤 식으로 작동하는가?

짧은 답변:

```text
condition variable은 어떤 조건이 만족될 때까지 thread를 잠들게 하는 도구입니다.
이 코드에서는 queue가 비었으면 worker가 잠들고,
main thread가 fd를 queue에 넣으면 worker를 깨웁니다.
반대로 queue가 가득 찼으면 submit 쪽이 잠들고,
worker가 fd를 꺼내면 submit 쪽을 깨웁니다.
```

이 코드에는 condition variable이 두 개 있습니다.

```text
not_empty
  queue가 비어 있지 않다는 조건을 기다립니다.
  worker thread가 사용합니다.

not_full
  queue가 가득 차지 않았다는 조건을 기다립니다.
  thread_pool_submit() 쪽이 사용합니다.
```

worker가 queue가 비었을 때 기다리는 코드:

```c
pthread_mutex_lock(&pool->mutex); // queue 상태를 안전하게 확인하기 위해 mutex를 잡습니다.
while (!pool->stopping && pool->count == 0) { // 종료 중이 아니고 queue가 비어 있으면 기다립니다.
    pthread_cond_wait(&pool->not_empty, &pool->mutex); // mutex를 잠시 풀고 잠들었다가, signal을 받으면 다시 mutex를 잡습니다.
} // 여기까지 왔다는 것은 종료 중이거나 queue에 fd가 생겼다는 뜻입니다.
```

main thread가 fd를 넣고 worker를 깨우는 코드:

```c
pool->fds[pool->tail] = client_fd; // queue의 tail 위치에 새 client fd를 넣습니다.
pool->tail = (pool->tail + 1) % pool->capacity; // tail을 다음 위치로 옮기고 끝이면 0으로 돌아갑니다.
pool->count++; // queue 안의 fd 개수를 1 늘립니다.
pthread_cond_signal(&pool->not_empty); // queue가 비어 있어서 잠든 worker 하나를 깨웁니다.
```

핵심 흐름:

```text
1. worker가 queue를 확인한다.
2. count == 0이면 할 일이 없다.
3. worker는 pthread_cond_wait()로 잠든다.
4. main thread가 새 client_fd를 queue에 넣는다.
5. main thread가 pthread_cond_signal(&not_empty)를 호출한다.
6. 잠들어 있던 worker 하나가 깨어난다.
7. worker는 다시 mutex를 잡고 count를 확인한 뒤 fd를 꺼낸다.
```

`pthread_cond_wait()`에서 가장 중요한 점:

```text
pthread_cond_wait(cond, mutex)는 잠들기 전에 mutex를 풀어줍니다.
그리고 signal을 받고 깨어날 때 mutex를 다시 잡은 상태로 돌아옵니다.
```

왜 mutex를 풀어줘야 할까요?

```text
worker가 queue가 비었다고 판단한 뒤 mutex를 잡은 채 잠들면,
main thread가 queue에 fd를 넣으려고 해도 mutex를 못 잡습니다.
그러면 아무도 queue를 채우지 못하고 deadlock이 됩니다.

그래서 pthread_cond_wait()는
"잠드는 동작"과 "mutex를 푸는 동작"을 원자적으로 함께 처리합니다.
```

왜 `if`가 아니라 `while`로 조건을 검사할까요?

```c
while (!pool->stopping && pool->count == 0) { // 깨어난 뒤에도 queue가 여전히 비었는지 다시 확인합니다.
    pthread_cond_wait(&pool->not_empty, &pool->mutex); // 조건이 아직 만족되지 않으면 다시 잠듭니다.
} // while을 빠져나오면 fd를 꺼낼 수 있거나 shutdown을 처리할 수 있습니다.
```

이유:

```text
깨어났다고 해서 조건이 반드시 만족된다는 보장은 없습니다.
여러 worker가 동시에 깨어날 수도 있고,
다른 worker가 먼저 fd를 가져갔을 수도 있습니다.
또 spurious wakeup이라고 해서 특별한 이유 없이 깨어날 수도 있습니다.

그래서 깨어난 뒤 반드시 while 조건을 다시 확인합니다.
```

발표용 한 문장:

```text
condition variable은 "조건이 아직 아니면 잠들고, 조건이 바뀌면 깨워주는 장치"이며,
이 코드에서는 queue가 비었는지 또는 가득 찼는지를 기준으로 thread를 재웁니다.
```

## Q4. read lock과 write lock의 구체적인 차이는 무엇인가?

짧은 답변:

```text
read lock은 여러 thread가 동시에 잡을 수 있습니다.
write lock은 한 thread만 잡을 수 있고, read lock과도 동시에 잡힐 수 없습니다.
즉 read lock은 공유 읽기용, write lock은 배타적 변경용입니다.
```

허용 관계:

| 현재 상태 | 새 read lock | 새 write lock |
| --- | --- | --- |
| 아무도 lock을 안 잡음 | 가능 | 가능 |
| read lock이 하나 이상 있음 | 가능 | 기다림 |
| write lock이 있음 | 기다림 | 기다림 |

이 코드에서는 `SELECT`가 read lock을 사용합니다.

```c
if (pthread_rwlock_rdlock(&db->lock) != 0) { // SELECT가 DB 상태를 읽기 전에 read lock을 요청합니다.
    return make_error_result("failed to acquire read lock", start_us); // lock 획득에 실패하면 에러 결과를 반환합니다.
} // 여기부터는 다른 SELECT와는 동시에 읽을 수 있지만 INSERT와는 겹치지 않습니다.
```

`INSERT`는 write lock을 사용합니다.

```c
if (pthread_rwlock_wrlock(&db->lock) != 0) { // INSERT가 DB 상태를 바꾸기 전에 write lock을 요청합니다.
    return make_error_result("failed to acquire write lock", start_us); // lock 획득에 실패하면 에러 결과를 반환합니다.
} // 여기부터는 SELECT와 다른 INSERT가 모두 기다리므로 혼자 DB를 수정합니다.
```

왜 SELECT는 read lock인가?

```text
SELECT는 records 배열과 B+ tree를 읽기만 합니다.
여러 SELECT가 동시에 같은 데이터를 읽어도 데이터가 깨지지 않습니다.
그래서 read lock을 사용하면 여러 조회를 동시에 허용할 수 있습니다.
```

왜 INSERT는 write lock인가?

```text
INSERT는 DB 상태를 바꿉니다.
다음 값들이 함께 변경됩니다.

1. data file에 새 줄 append
2. records 배열에 새 Record 추가
3. count 증가
4. next_id 증가
5. B+ tree에 id -> record_index 추가

이 작업 중간에 다른 thread가 끼어들면 DB 상태가 깨질 수 있습니다.
그래서 write lock을 사용해 INSERT 전체를 혼자 실행하게 합니다.
```

예시:

```text
SELECT + SELECT
  둘 다 read lock입니다.
  동시에 실행 가능합니다.

SELECT + INSERT
  SELECT는 read lock, INSERT는 write lock입니다.
  동시에 실행되지 않습니다.

INSERT + INSERT
  둘 다 write lock입니다.
  동시에 실행되지 않습니다.
```

왜 그냥 mutex 하나만 쓰지 않았을까요?

```text
mutex 하나만 쓰면 SELECT끼리도 서로 기다려야 합니다.
하지만 SELECT는 읽기만 하므로 동시에 실행해도 됩니다.

read-write lock을 쓰면 읽기 작업은 병렬성을 살리고,
쓰기 작업만 배타적으로 막을 수 있습니다.
```

발표용 한 문장:

```text
read lock은 "여러 명이 같이 읽어도 된다"는 lock이고,
write lock은 "지금은 내가 상태를 바꾸는 중이니 아무도 들어오면 안 된다"는 lock입니다.
```

## Q5. 이 서버에서 동시성 처리를 한 문장으로 설명하면?

답변:

```text
요청을 worker에게 나눠주는 queue는 mutex와 condition variable로 보호하고,
여러 worker가 공유하는 DB 상태는 read-write lock으로 보호합니다.
```

조금 더 발표답게 말하면:

```text
네트워크 요청 분배와 DB 데이터 보호를 분리해서 설계했습니다.
thread pool queue는 짧게 잠그는 mutex로 보호하고,
DB engine은 SELECT 병렬성과 INSERT 안전성을 동시에 얻기 위해 pthread_rwlock_t를 사용했습니다.
```

## Q6. lock이 없다면 실제로 어떤 문제가 생길 수 있나?

답변:

```text
thread pool queue에서는 같은 client_fd를 두 worker가 동시에 꺼내거나,
queue count가 실제 상태와 달라질 수 있습니다.

DB engine에서는 두 INSERT가 같은 next_id를 읽어 id가 중복될 수 있고,
records 배열 같은 위치에 서로 다른 record를 덮어쓸 수 있으며,
B+ tree가 잘못된 record index를 가리킬 수 있습니다.
```

INSERT race 예시:

```text
초기 상태:
  next_id = 10
  count = 9

thread 1:
  next_id를 읽어서 id = 10으로 결정
  count를 읽어서 record_index = 9로 결정

thread 2:
  thread 1이 반영하기 전에 next_id를 읽어서 id = 10으로 결정
  count를 읽어서 record_index = 9로 결정

결과:
  같은 id가 생기거나,
  같은 records[9] 위치를 덮어쓰거나,
  B+ tree index가 실제 records 배열과 어긋날 수 있습니다.
```

현재 코드는 `db_insert()` 시작부터 끝까지 write lock을 잡기 때문에 이 상황을 막습니다.

## Q7. 발표에서 이 내용을 어떻게 시연하면 좋나?

답변:

서버를 worker 4개로 실행합니다.

```sh
./bin/week8_dbms 8080 4 data/demo_users.csv
```

다른 터미널에서 동시에 INSERT 요청을 보냅니다.

```sh
seq 1 100 | xargs -P 20 -I{} curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data "{\"sql\":\"INSERT INTO users name age VALUES 'user{}' 20;\"}" >/dev/null
```

서버 로그에서 확인합니다.

```text
[thread=...] 값이 여러 개로 나옵니다.
즉 여러 worker thread가 요청을 나누어 처리하고 있습니다.
```

그리고 조회합니다.

```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data '{"sql":"SELECT * FROM users WHERE id = 1;"}'
```

발표에서 말할 포인트:

```text
동시에 여러 요청이 들어오지만 worker thread들은 queue를 안전하게 공유합니다.
그리고 여러 worker가 같은 DB engine을 사용하지만,
INSERT는 write lock으로 보호되기 때문에 id, records 배열, 파일, B+ tree가 함께 일관되게 갱신됩니다.
```
