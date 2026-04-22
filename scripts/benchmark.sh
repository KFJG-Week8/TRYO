#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-8080}"
COUNT="${2:-1000}"
BASE_URL="http://127.0.0.1:${PORT}"
BASE_ID="$(date +%s)"

post_sql() {
  local sql="$1"
  curl -s -X POST "${BASE_URL}/query" \
    -H 'Content-Type: text/plain' \
    --data "${sql}"
}

echo "Health:"
curl -s "${BASE_URL}/health"
echo

echo "Inserting ${COUNT} users..."
for i in $(seq 1 "${COUNT}"); do
  post_sql "INSERT INTO users VALUES ($((BASE_ID + i)), 'user${i}', $((20 + (i % 30))));"
  if (( i % 100 == 0 )); then
    echo "inserted ${i}"
  fi
done

echo "Indexed lookup by id:"
post_sql "SELECT * FROM users WHERE id = $((BASE_ID + COUNT));"
echo

echo "Linear lookup by name:"
post_sql "SELECT id, name FROM users WHERE name = 'user${COUNT}';"
echo

echo "Concurrent health requests:"
for _ in $(seq 1 20); do
  curl -s "${BASE_URL}/health" >/dev/null &
done
wait
echo "done"
