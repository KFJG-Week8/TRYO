# WEEK8 Mini DBMS API Server

C implementation of a tiny file-backed DBMS exposed through an HTTP API server. It is designed for the WEEK8 coding meeting around BSD sockets, TCP, HTTP, file descriptors, thread pools, and simple DB indexing.

## Build
```sh
make
```

## Run
```sh
./bin/week8_dbms
```

Optional arguments:
```sh
./bin/week8_dbms [port] [thread_count] [data_file]
```

Example:
```sh
./bin/week8_dbms 8080 4 data/users.csv
```

## API Test
Health check:
```sh
curl http://127.0.0.1:8080/health
```

Insert:
```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data '{"sql":"INSERT INTO users name age VALUES '\''kim'\'' 20;"}'
```

Select all:
```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data '{"sql":"SELECT * FROM users;"}'
```

Select by indexed id:
```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data '{"sql":"SELECT * FROM users WHERE id = 1;"}'
```

Select by linear scan:
```sh
curl -s -X POST http://127.0.0.1:8080/query \
  -H 'Content-Type: application/json' \
  --data '{"sql":"SELECT * FROM users WHERE name = '\''kim'\'';"}'
```

## Tests
```sh
make test
```

The tests cover:
- supported SQL parsing
- unsupported SQL rejection
- B+ tree insert/search
- file-backed DB insert/select/reload behavior
- JSON SQL extraction

## Benchmark
Start the server first, then run:

```sh
bash scripts/benchmark.sh 8080 1000
```

The script inserts sample users, runs concurrent requests, and compares an indexed `WHERE id` lookup with a linear `WHERE name` lookup.

## Supported SQL
```sql
INSERT INTO users name age VALUES 'kim' 20;
SELECT * FROM users;
SELECT * FROM users WHERE id = 1;
SELECT * FROM users WHERE name = 'kim';
```

## Project Shape
```text
include/          public headers
src/              implementation
tests/            unit and integration-style C tests
scripts/          benchmark/demo helpers
data/             CSV data files
```

## Notes
This is a learning-oriented implementation, not a production HTTP server or SQL database. It intentionally keeps the grammar, JSON handling, and HTTP behavior small so the socket, thread-pool, DB, and B+ tree flow remains visible.
