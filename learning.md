# Learning Guide

## 1. Follow the file descriptor
Start in `src/main.c`, then move to `src/server.c`.

Look for this sequence:

```text
socket() -> bind() -> listen() -> accept() -> worker queue -> recv() -> send() -> close()
```

This is the core CS:APP Network Programming path.

## 2. Separate TCP from HTTP
TCP only gives the server a byte stream. HTTP is parsed manually from those bytes in `src/http.c`.

Important details:
- The first line gives method and path.
- Headers end at `\r\n\r\n`.
- `Content-Length` tells the server how many body bytes to read.
- The response is also plain text written to the socket fd.

## 3. Watch the thread handoff
`accept()` returns a client fd in the main thread. The thread pool stores that fd in a queue protected by a mutex and condition variables. A worker later wakes up and becomes responsible for that fd.

Key code:
- queue push: `thread_pool_submit`
- queue pop: worker loop in `src/thread_pool.c`
- request handling: `handle_client` in `src/server.c`

## 4. Trace SQL execution
For `POST /query`, the flow is:

```text
JSON body -> SQL string -> SqlStatement -> DB operation -> JSON rows
```

The SQL parser does not execute anything. It only turns text into a structured statement.

## 5. Compare indexed and linear search
`SELECT * FROM users WHERE id = N;` calls `bptree_search`.

`SELECT * FROM users WHERE name = 'kim';` scans the record array.

The API response and server log both show whether the index was used and how long the DB operation took.

## 6. Understand the lock choice
The DB engine uses a global read-write lock:

- Many SELECT requests can run together.
- INSERT blocks other DB work while it appends to the file, updates the record array, and updates the B+ tree.

This is simpler than table-level locking and much easier to explain during a coding meeting.
