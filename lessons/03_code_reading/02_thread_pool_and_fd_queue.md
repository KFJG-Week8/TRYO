# 03-02. Thread Pool과 File Descriptor Queue

## 먼저 한 문장으로 보기
`thread_pool.c`는 main thread가 받은 client fd를 worker thread에게 안전하게 넘겨, 여러 요청을 병렬로 처리하게 만드는 모듈입니다.

## 요청 흐름에서의 위치
```text
accept()가 client_fd를 반환
  -> thread_pool_submit(client_fd)
  -> queue에 저장
  -> worker_loop가 queue에서 꺼냄
  -> handle_client(client_fd)
```

## 이 코드를 읽기 전에 알아야 할 CS 개념
| 개념 | 짧은 설명 | 이 파일에서 보이는 지점 |
| --- | --- | --- |
| thread | 같은 process 안에서 독립적으로 실행되는 흐름입니다. | `pthread_create()` |
| shared queue | 여러 thread가 함께 접근하는 작업 목록입니다. | `pool->fds` |
| mutex | 공유 상태를 동시에 바꾸지 못하게 보호합니다. | `pthread_mutex_lock()` |
| condition variable | 조건이 만족될 때까지 thread를 잠들게 합니다. | `not_empty`, `not_full` |
| backpressure | 처리량보다 요청이 많을 때 앞단을 기다리게 하는 압력입니다. | queue full 대기 |

## client fd는 무엇인가
client fd는 특정 클라이언트 연결과 통신할 수 있는 번호입니다. 이 번호를 worker에게 넘긴다는 것은 “이 클라이언트 요청을 네가 처리하라”는 권한을 넘기는 것과 비슷합니다.

중요한 점은 fd 자체가 요청 데이터가 아니라는 것입니다. fd는 데이터를 읽고 쓸 수 있는 통로입니다. worker는 이 fd로 `recv()`를 호출해 요청을 읽고, `send()`로 응답을 보냅니다.

## API 카드: `thread_pool_init`
- 이름: `thread_pool_init`
- 위치: `src/thread_pool.c`, 선언은 `include/thread_pool.h`
- 한 문장 목적: 고정된 개수의 worker thread와 fd queue를 초기화합니다.
- 입력:
  - `ThreadPool *pool`
  - `size_t thread_count`
  - `size_t queue_capacity`
  - `TaskHandler handler`
  - `void *context`
- 출력: 성공 시 `1`, 실패 시 `0`
- 호출되는 시점: `server_run()`에서 서버 accept loop가 시작되기 전
- 내부에서 하는 일:
  - fd queue 배열을 할당합니다.
  - worker thread 배열을 할당합니다.
  - mutex와 condition variable을 초기화합니다.
  - `pthread_create()`로 worker들을 시작합니다.
- 실패할 수 있는 지점:
  - 메모리 할당 실패
  - mutex/condition variable 초기화 실패
  - thread 생성 실패
- 학습자가 확인할 질문:
  - worker thread를 요청이 들어올 때마다 만들지 않고 미리 만드는 이유는 무엇일까요?
  - `handler` 함수 포인터는 왜 필요할까요?

## API 카드: `thread_pool_submit`
- 이름: `thread_pool_submit`
- 위치: `src/thread_pool.c`
- 한 문장 목적: main thread가 얻은 client fd를 worker들이 공유하는 queue에 넣습니다.
- 입력: `ThreadPool *pool`, `int client_fd`
- 출력: 성공 시 `1`, 종료 중이면 `0`
- 호출되는 시점: `server_run()`의 accept loop에서 `accept()` 성공 직후
- 내부에서 하는 일:
  - mutex를 잡습니다.
  - queue가 가득 차 있으면 `not_full` condition variable에서 기다립니다.
  - queue tail 위치에 fd를 넣습니다.
  - `not_empty`를 signal해서 잠든 worker를 깨웁니다.
  - mutex를 풉니다.
- 실패할 수 있는 지점:
  - thread pool이 종료 중인 경우
- 학습자가 확인할 질문:
  - queue가 가득 찬 상황을 고려하지 않으면 어떤 일이 생길까요?
  - fd를 queue에 넣는 동안 mutex가 필요한 이유는 무엇일까요?

## API 카드: `worker_loop`
- 이름: `worker_loop`
- 위치: `src/thread_pool.c`
- 한 문장 목적: worker thread가 queue에서 client fd를 꺼내 handler로 처리하는 반복 루프입니다.
- 입력: `void *arg`, 실제로는 `ThreadPool *`
- 출력: thread 종료 시 `NULL`
- 호출되는 시점: `pthread_create()`가 worker thread를 시작할 때
- 내부에서 하는 일:
  - mutex를 잡습니다.
  - queue가 비어 있으면 `not_empty`에서 기다립니다.
  - fd를 하나 꺼냅니다.
  - `not_full`을 signal해서 submit 쪽을 깨울 수 있게 합니다.
  - mutex를 풉니다.
  - `pool->handler(client_fd, pool->context)`를 호출합니다.
- 실패할 수 있는 지점:
  - 직접 에러를 반환하지는 않습니다. 종료 flag와 queue 상태를 보고 루프를 빠져나갑니다.
- 학습자가 확인할 질문:
  - 왜 `handler` 호출은 mutex를 푼 뒤에 할까요?
  - worker가 queue가 비었을 때 busy waiting하지 않는 이유는 무엇일까요?

## API 카드: `thread_pool_shutdown`
- 이름: `thread_pool_shutdown`
- 위치: `src/thread_pool.c`
- 한 문장 목적: worker들에게 종료를 알리고 thread, queue, 동기화 자원을 정리합니다.
- 입력: `ThreadPool *pool`
- 출력: 없음
- 호출되는 시점: 서버 종료 또는 초기화 실패 처리 중
- 내부에서 하는 일:
  - `stopping` flag를 켭니다.
  - condition variable을 broadcast해서 잠든 thread를 깨웁니다.
  - 모든 worker를 `pthread_join()`으로 기다립니다.
  - condition variable, mutex, 메모리를 해제합니다.
- 실패할 수 있는 지점:
  - 현재 구현은 정리 실패를 별도로 보고하지 않습니다.
- 학습자가 확인할 질문:
  - 잠든 worker를 깨우지 않으면 shutdown은 왜 멈출 수 있을까요?

## mutex와 condition variable의 역할
mutex는 “동시에 건드리면 안 되는 queue 상태”를 보호합니다.

보호 대상:

- `head`
- `tail`
- `count`
- `stopping`
- `fds[]`

condition variable은 “조건이 만족될 때까지 thread를 재우는 장치”입니다.

- `not_empty`: queue에 fd가 들어오면 worker를 깨웁니다.
- `not_full`: queue에 빈자리가 생기면 submit 쪽을 깨웁니다.

## 이 구현의 설계 선택
이 프로젝트는 bounded queue를 사용합니다. queue capacity는 무한하지 않습니다. 이는 실제 서버 설계에서 중요한 질문을 만듭니다.

요청이 너무 많이 들어오면 어떻게 할 것인가?

이 구현은 queue가 가득 차면 submit 쪽이 기다립니다. 학습용으로는 단순하고 안전합니다. 실제 서버라면 timeout, reject, backpressure, event loop 같은 선택지가 생깁니다.

## 코드 관찰 포인트
- `worker_loop()`는 queue에서 fd를 꺼낸 뒤 mutex를 풀고 handler를 호출합니다. handler 실행 중 queue 전체가 잠기지 않게 하기 위해서입니다.
- `thread_pool_submit()`은 queue가 꽉 찼을 때 무작정 실패하지 않고 기다립니다.
- `thread_pool_shutdown()`은 잠든 worker를 깨우기 위해 condition variable을 broadcast합니다.

## 흔한 오해
| 오해 | 바로잡기 |
| --- | --- |
| mutex는 thread를 빠르게 만든다. | mutex는 빠르게 만드는 도구가 아니라 공유 상태를 안전하게 만드는 도구입니다. |
| condition variable은 작업을 저장한다. | condition variable은 저장소가 아니라 잠든 thread를 깨우는 신호 장치입니다. |
| fd queue에는 HTTP request가 들어 있다. | queue에는 request body가 아니라 client fd가 들어 있습니다. request는 worker가 fd에서 읽습니다. |

## 다음 문서로 넘어가기
이제 worker가 fd를 받았을 때, 그 fd에서 HTTP 요청을 어떻게 읽는지 봅니다.

다음: [03_http_and_json.md](03_http_and_json.md)
