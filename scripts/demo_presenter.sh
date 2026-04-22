#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-8080}"
THREADS="${2:-4}"
DATA_FILE="${3:-data/demo_users.csv}"
COUNT="${DEMO_COUNT:-40}"
PARALLEL="${DEMO_PARALLEL:-12}"
BASE_URL="http://127.0.0.1:${PORT}"

usage() {
  cat <<USAGE
Usage:
  bash scripts/demo_presenter.sh [port] [thread_count] [data_file]

Example:
  bash scripts/demo_presenter.sh 8080 4 data/demo_users.csv

This is a presenter helper. It prints each demo command before running it,
then waits for Enter so the demo still feels manual and explainable.

Environment overrides:
  DEMO_COUNT=100 DEMO_PARALLEL=30 bash scripts/demo_presenter.sh
USAGE
}

print_command() {
  printf "\n---- %s ----\n" "$1"
  printf "%s\n" "$2"
}

prompt_step() {
  local title="$1"
  local command="$2"

  print_command "$title" "$command"
  printf "Press Enter to run, or type s then Enter to skip: "
  local answer
  read -r answer
  if [[ "$answer" == "s" || "$answer" == "S" ]]; then
    printf "Skipped.\n"
    return 1
  fi
  return 0
}

manual_step() {
  local title="$1"
  local command="$2"

  print_command "$title" "$command"
  printf "Run this in Terminal A, then press Enter here to continue: "
  read -r _
}

quote() {
  printf "%q" "$1"
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

cat <<INTRO
WEEK8 mini DBMS API server demo helper

Terminal A: server log screen
Terminal B: run this helper

Settings:
  PORT=${PORT}
  THREADS=${THREADS}
  DATA_FILE=${DATA_FILE}
  COUNT=${COUNT}
  PARALLEL=${PARALLEL}
INTRO

if prompt_step "1. Internal tests" "make test"; then
  make test
  printf "\n"
fi

if prompt_step "2. Clean demo data" "rm -f $(quote "${DATA_FILE}")"; then
  rm -f "${DATA_FILE}"
  printf "\n"
fi

manual_step "3. Start server in Terminal A" "./bin/week8_dbms ${PORT} ${THREADS} ${DATA_FILE}"

if prompt_step "4. Health check" "curl ${BASE_URL}/health"; then
  curl "${BASE_URL}/health"
  printf "\n"
fi

insert_json="{\"sql\":\"INSERT INTO users name age VALUES 'kim' 20;\"}"
insert_display="curl -s -X POST ${BASE_URL}/query -H 'Content-Type: application/json' --data \"{\\\"sql\\\":\\\"INSERT INTO users name age VALUES 'kim' 20;\\\"}\""
if prompt_step "5. INSERT through API" "${insert_display}"; then
  curl -s -X POST "${BASE_URL}/query" \
    -H 'Content-Type: application/json' \
    --data "${insert_json}"
  printf "\n"
fi

select_json="{\"sql\":\"SELECT * FROM users;\"}"
select_display="curl -s -X POST ${BASE_URL}/query -H 'Content-Type: application/json' --data \"{\\\"sql\\\":\\\"SELECT * FROM users;\\\"}\""
if prompt_step "6. SELECT through API" "${select_display}"; then
  curl -s -X POST "${BASE_URL}/query" \
    -H 'Content-Type: application/json' \
    --data "${select_json}"
  printf "\n"
fi

if prompt_step "7. Parallel INSERT demo" "bash scripts/concurrency_demo.sh ${PORT} ${COUNT} ${PARALLEL}"; then
  bash scripts/concurrency_demo.sh "${PORT}" "${COUNT}" "${PARALLEL}"
  printf "\n"
fi

verify_json="{\"sql\":\"SELECT * FROM users WHERE name = 'parallel${COUNT}';\"}"
verify_display="curl -s -X POST ${BASE_URL}/query -H 'Content-Type: application/json' --data \"{\\\"sql\\\":\\\"SELECT * FROM users WHERE name = 'parallel${COUNT}';\\\"}\""
if prompt_step "8. Verify parallel row" "${verify_display}"; then
  curl -s -X POST "${BASE_URL}/query" \
    -H 'Content-Type: application/json' \
    --data "${verify_json}"
  printf "\n"
fi

unsupported_json="{\"sql\":\"DELETE FROM users WHERE id = 1;\"}"
unsupported_display="curl -s -X POST ${BASE_URL}/query -H 'Content-Type: application/json' --data \"{\\\"sql\\\":\\\"DELETE FROM users WHERE id = 1;\\\"}\""
if prompt_step "9. Unsupported SQL error" "${unsupported_display}"; then
  curl -s -X POST "${BASE_URL}/query" \
    -H 'Content-Type: application/json' \
    --data "${unsupported_json}"
  printf "\n"
fi

wrong_key_json="{\"query\":\"SELECT * FROM users;\"}"
wrong_key_display="curl -s -X POST ${BASE_URL}/query -H 'Content-Type: application/json' --data \"{\\\"query\\\":\\\"SELECT * FROM users;\\\"}\""
if prompt_step "10. Wrong JSON key error" "${wrong_key_display}"; then
  curl -s -X POST "${BASE_URL}/query" \
    -H 'Content-Type: application/json' \
    --data "${wrong_key_json}"
  printf "\n"
fi

cat <<OUTRO
Demo helper finished.
Stop the server in Terminal A with Ctrl+C.
OUTRO
