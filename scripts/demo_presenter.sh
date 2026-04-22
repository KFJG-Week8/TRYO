#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-8080}"
THREADS="${2:-4}"
DATA_FILE="${3:-data/demo_users.csv}"
COUNT="${DEMO_COUNT:-40}"
PARALLEL="${DEMO_PARALLEL:-12}"
BASE_URL="http://127.0.0.1:${PORT}"
RUN_LABEL="${DEMO_RUN_LABEL:-run$(date +%H%M%S)}"
RUN_LABEL="${RUN_LABEL//[^A-Za-z0-9_]/_}"

if [[ -z "${RUN_LABEL}" ]]; then
  RUN_LABEL="run$(date +%H%M%S)"
fi

usage() {
  cat <<USAGE
Usage:
  bash scripts/demo_presenter.sh [port] [thread_count] [data_file]

Example:
  bash scripts/demo_presenter.sh 8080 4 data/demo_users.csv

This is a presenter helper. It prints each demo command before running it,
then waits for Enter so the demo still feels manual and explainable.
Start the server in another terminal before running this helper.

Environment overrides:
  DEMO_COUNT=100 DEMO_PARALLEL=30 bash scripts/demo_presenter.sh
USAGE
}

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

clear_prompt_line() {
  if [[ -t 1 ]]; then
    printf "\033[1A\033[2K"
  else
    printf "\n"
  fi
}

prompt_step() {
  local title="$1"
  local command="$2"

  print_header "$title"
  print_command "$command"
  printf "\nPress Enter to run this command, or type s then Enter to skip: "
  local answer
  read -r answer
  clear_prompt_line
  if [[ "$answer" == "s" || "$answer" == "S" ]]; then
    printf "Skipped: %s\n" "$title"
    return 1
  fi
  return 0
}

run_command_output() {
  local status

  set +e
  "$@" 2>&1 | awk '
    BEGIN { printed = 0 }
    {
      if (!printed) {
        print ""
        print "Output"
        print "------"
        printed = 1
      }
      print
    }
  '
  status=${PIPESTATUS[0]}
  set -e
  return "${status}"
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

print_header "WEEK8 mini DBMS API server demo"
cat <<INTRO
Terminal layout
  Terminal A  already-running server
  Terminal B  this presenter helper

Settings
  Port        ${PORT}
  Workers     ${THREADS}
  Data file   ${DATA_FILE}
  Requests    ${COUNT}
  Parallel    ${PARALLEL}
  Run label   ${RUN_LABEL}
INTRO

if prompt_step "1. Health check" "curl ${BASE_URL}/health"; then
  run_command_output curl "${BASE_URL}/health"
fi

insert_sql="INSERT INTO users VALUES (1, 'kim', 20);"
insert_display="curl -s -X POST ${BASE_URL}/query -H 'Content-Type: text/plain' --data \"${insert_sql}\""
if prompt_step "2. INSERT through API" "${insert_display}"; then
  run_command_output curl -s -X POST "${BASE_URL}/query" \
    -H 'Content-Type: text/plain' \
    --data "${insert_sql}"
fi

select_sql="SELECT id, name FROM users;"
select_display="curl -s -X POST ${BASE_URL}/query -H 'Content-Type: text/plain' --data \"${select_sql}\""
if prompt_step "3. SELECT columns through API" "${select_display}"; then
  run_command_output curl -s -X POST "${BASE_URL}/query" \
    -H 'Content-Type: text/plain' \
    --data "${select_sql}"
fi

if prompt_step "4. Parallel INSERT demo" "DEMO_RUN_LABEL=${RUN_LABEL} bash scripts/concurrency_demo.sh ${PORT} ${COUNT} ${PARALLEL}"; then
  run_command_output env "DEMO_RUN_LABEL=${RUN_LABEL}" bash scripts/concurrency_demo.sh "${PORT}" "${COUNT}" "${PARALLEL}"
fi

verify_sql="SELECT * FROM users WHERE name = 'parallel_${RUN_LABEL}_${COUNT}';"
verify_display="curl -s -X POST ${BASE_URL}/query -H 'Content-Type: text/plain' --data \"${verify_sql}\""
if prompt_step "5. Verify stored parallel data" "${verify_display}"; then
  run_command_output curl -s -X POST "${BASE_URL}/query" \
    -H 'Content-Type: text/plain' \
    --data "${verify_sql}"
fi

print_header "Demo helper finished"
cat <<OUTRO
Next step
  Stop the server in Terminal A when the demo is finished.
OUTRO
