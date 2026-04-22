#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-8080}"
COUNT="${2:-1000}"
BASE_URL="http://127.0.0.1:${PORT}"
BASE_ID="$(date +%s)"

line() {
  printf "%s\n" "------------------------------------------------------------"
}

print_header() {
  printf "\n"
  line
  printf "%s\n" "$1"
  line
}

print_command() {
  printf "\nCommand\n"
  printf "  $ %s\n" "$1"
}

post_sql() {
  local sql="$1"
  curl -s -X POST "${BASE_URL}/query" \
    -H 'Content-Type: text/plain' \
    --data "${sql}"
}

print_header "Benchmark demo"
cat <<INTRO
Settings
  Base URL    ${BASE_URL}
  Inserts     ${COUNT}
INTRO

print_header "Health check"
print_command "curl -s ${BASE_URL}/health"
curl -s "${BASE_URL}/health"
printf "\n"

print_header "Bulk INSERT"
print_command "curl -s -X POST ${BASE_URL}/query -H 'Content-Type: text/plain' --data \"INSERT INTO users VALUES (...);\""
printf "\nInserting %s users...\n" "${COUNT}"
for i in $(seq 1 "${COUNT}"); do
  post_sql "INSERT INTO users VALUES ($((BASE_ID + i)), 'user${i}', $((20 + (i % 30))));"
  if (( i % 100 == 0 )); then
    printf "  inserted %s / %s\n" "${i}" "${COUNT}"
  fi
done

print_header "Indexed lookup by id"
print_command "curl -s -X POST ${BASE_URL}/query -H 'Content-Type: text/plain' --data \"SELECT * FROM users WHERE id = ...;\""
post_sql "SELECT * FROM users WHERE id = $((BASE_ID + COUNT));"
printf "\n"

print_header "Linear lookup by name"
print_command "curl -s -X POST ${BASE_URL}/query -H 'Content-Type: text/plain' --data \"SELECT id, name FROM users WHERE name = 'user...';\""
post_sql "SELECT id, name FROM users WHERE name = 'user${COUNT}';"
printf "\n"

print_header "Concurrent health requests"
print_command "curl -s ${BASE_URL}/health"
for _ in $(seq 1 20); do
  curl -s "${BASE_URL}/health" >/dev/null &
done
wait
printf "\nDone. Sent 20 concurrent health requests.\n"
