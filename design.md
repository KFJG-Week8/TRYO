# Design

## Architecture
The runtime flow follows the WEEK8 network-programming path:

```text
client
  -> TCP connect
  -> server accept()
  -> client fd enqueue
  -> worker thread dequeue
  -> HTTP parser
  -> SQL parser
  -> DB engine
  -> HTTP JSON response
  -> close client fd
```

The main thread owns the listening socket. Worker threads own accepted client sockets after they are pushed into the thread-pool queue.

## Modules
- `src/main.c`: command-line arguments and server startup.
- `src/server.c`: BSD socket setup, accept loop, routing, and request logging.
- `src/thread_pool.c`: fixed-size worker pool and bounded fd queue.
- `src/http.c`: minimal HTTP request reader and JSON response writer.
- `src/sql.c`: minimal SQL parser for the supported grammar.
- `src/db.c`: file-backed `users` table, read-write locking, and query execution helpers.
- `src/bptree.c`: in-memory B+ tree mapping `id -> record index`.

## API Boundary
The network layer does not know file formats or index internals. It receives SQL text, calls the SQL parser, converts the parsed statement into a DB operation, and serializes the DB result as JSON.

## Concurrency Model
The server is concurrent at the request level:

- `accept()` runs in the main thread.
- Each accepted client file descriptor is placed in the thread-pool queue.
- Workers process requests independently.

The DB engine is protected by one `pthread_rwlock_t`:

- `SELECT` uses `pthread_rwlock_rdlock`.
- `INSERT` uses `pthread_rwlock_wrlock`.

This allows multiple reads at once while keeping append writes and B+ tree updates consistent.

## Persistence and Startup
The data file uses one record per line:

```text
1,kim,20
2,lee,22
```

On startup, the DB engine reads every line, stores records in memory, tracks the next auto-increment id, and inserts every id into the B+ tree.

## B+ Tree Usage
The B+ tree stores only integer ids and record-array positions. It is used only for:

```sql
SELECT * FROM users WHERE id = N;
```

All other searches scan the record array. This makes the index effect easy to demonstrate in logs and benchmarks.

## Failure Behavior
Unsupported SQL, malformed JSON, bad HTTP requests, missing routes, and DB errors return JSON error bodies. The server closes every client connection after one response, so keep-alive is intentionally not implemented.
