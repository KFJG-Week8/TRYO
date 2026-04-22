# WEEK8 Mini DBMS API Server Requirements

## Goal
C language mini DBMS API server using BSD sockets. External clients send SQL through HTTP, and the server executes it with a file-based DB engine plus an in-memory B+ tree index.

This project is intentionally small enough to finish in one bootcamp day, while still exposing the main WEEK8 concepts: TCP sockets, file descriptors, HTTP, thread pools, and DB concurrency.

## Core Learning Keywords
- BSD socket
- IP / TCP
- HTTP request and response
- file descriptor
- DNS / localhost
- thread pool
- concurrency control
- file-based DB
- SQL parser
- B+ tree index

## Supported API
### GET /health
Server health check.

Response:
```json
{"status":"ok"}
```

### POST /query
SQL execution API.

Request:
```json
{"sql":"SELECT * FROM users WHERE id = 1;"}
```

Success response:
```json
{"ok":true,"rows":[],"message":"success","index_used":false,"elapsed_us":10}
```

Error response:
```json
{"ok":false,"error":"reason"}
```

## Supported SQL
- `INSERT INTO users name age VALUES 'kim' 20;`
- `SELECT * FROM users;`
- `SELECT * FROM users WHERE id = 1;`
- `SELECT * FROM users WHERE name = 'kim';`

## Table Assumption
Only one table exists: `users`.

Columns:
- `id`: auto-increment integer
- `name`: string
- `age`: integer

## Storage
- Data is stored in a CSV-like file: `id,name,age`.
- On server startup, the DB engine reads the file and rebuilds the in-memory record array and B+ tree index.
- `INSERT` appends one line to the data file.

## Index
- The `id` column is indexed by an in-memory B+ tree.
- `WHERE id = ?` uses the B+ tree.
- Other searches use linear scan.

## Concurrency
- The server uses a fixed-size thread pool.
- The accept loop pushes client file descriptors into a bounded queue.
- Worker threads pop client file descriptors, parse HTTP, execute SQL, write the response, and close the descriptor.
- The DB engine uses one global read-write lock.
- `SELECT` takes a read lock.
- `INSERT` takes a write lock.

## Out of Scope
- `CREATE TABLE`
- `UPDATE`
- `DELETE`
- `JOIN`
- Complex `WHERE` conditions
- Full SQL grammar
- Full JSON parser
- HTTPS
- HTTP keep-alive
- Production-grade HTTP behavior

## Success Criteria
- `make` builds the server.
- `make test` passes parser, B+ tree, and DB tests.
- The server accepts curl requests for health, insert, select-all, select-by-id, and select-by-name.
- Concurrent requests do not corrupt the file or index.
- Logs or benchmark results can explain the difference between indexed lookup and linear scan.
