#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-8080}"
COUNT="${2:-40}"
PARALLEL="${3:-12}"
BASE_URL="http://127.0.0.1:${PORT}"
TMP_FILE="$(mktemp)"
TMP_DIR="$(mktemp -d)"
RUN_LABEL="${DEMO_RUN_LABEL:-run$(date +%H%M%S)}"
RUN_LABEL="${RUN_LABEL//[^A-Za-z0-9_]/_}"

if [[ -z "${RUN_LABEL}" ]]; then
  RUN_LABEL="run$(date +%H%M%S)"
fi

RANDOM_U32="$(od -An -N4 -tu4 /dev/urandom | tr -d '[:space:]')"
MAX_BASE=$((2000000000 - COUNT - 1))
BASE_ID="${DEMO_BASE_ID:-$((10000 + (RANDOM_U32 % (MAX_BASE - 10000))))}"

export BASE_URL BASE_ID RUN_LABEL TMP_DIR

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
  printf "\nCommand pattern\n"
  printf "  $ %s\n" "$1"
}

cleanup() {
  rm -f "${TMP_FILE}"
  rm -rf "${TMP_DIR}"
}

trap cleanup EXIT

print_header "Concurrency demo"
cat <<INTRO
Settings
  Base URL    ${BASE_URL}
  Requests    ${COUNT}
  Parallel    ${PARALLEL}
  Run label   ${RUN_LABEL}
  Base id     ${BASE_ID}
INTRO

print_header "Health check"
print_command "curl -s ${BASE_URL}/health"
curl -s "${BASE_URL}/health"
printf "\n"

print_header "Parallel INSERT requests"
print_command "curl -s -X POST ${BASE_URL}/query -H 'Content-Type: text/plain' --data \"INSERT INTO users VALUES (..., 'parallel_${RUN_LABEL}_...', ...);\""
printf "\nSending %s INSERT requests with parallelism %s...\n" "${COUNT}" "${PARALLEL}"
seq 1 "${COUNT}" | xargs -P "${PARALLEL}" -I{} sh -c '
  i="$1"
  id=$((BASE_ID + i))
  age=$((20 + (i % 10)))
  headers_file="${TMP_DIR}/headers_${i}.txt"
  body_file="${TMP_DIR}/body_${i}.json"
  status="$(curl -s -D "${headers_file}" -o "${body_file}" -w "%{http_code}" -X POST "${BASE_URL}/query" \
    -H "Content-Type: text/plain" \
    --data "INSERT INTO users VALUES (${id}, '\''parallel_${RUN_LABEL}_${i}'\'', ${age});")"
  thread_id="$(awk '\''tolower($1) == "x-worker-thread-id:" { gsub("\r", "", $2); print $2; found = 1; exit } END { if (!found) print "missing" }'\'' "${headers_file}")"
  body="$(tr -d "\n" < "${body_file}")"
  printf "%s\t%s\t%s\t%s\t%s\n" "${i}" "${id}" "${thread_id}" "${status}" "${body}"
' _ {} > "${TMP_FILE}"

OK_COUNT="$(awk '
{
  if ($4 == "200" && $5 ~ /^\[/) {
    count++
  }
}
END {
  print count + 0
}' "${TMP_FILE}")"

print_header "Result"
printf "Successful responses  %s / %s\n" "${OK_COUNT}" "${COUNT}"
printf "\nWorker thread distribution returned to the client\n"
printf "%s\n" "-----------------------------------------------"
awk -F '\t' '
$3 != "" && $3 != "missing" {
  count[$3]++
  seen = 1
}
END {
  if (!seen) {
    print "  no worker thread ids returned"
    exit
  }
  for (thread_id in count) {
    printf "  thread=%s  responses=%d\n", thread_id, count[thread_id]
  }
}
' "${TMP_FILE}" | sort
printf "\nSample client-side INSERT responses\n"
printf "%s\n" "---------------"
awk -F '\t' '
shown < 8 {
  printf "  request=%s row_id=%s thread=%s body=%s\n", $1, $2, $3, $5
  shown++
}
' "${TMP_FILE}"
printf "\nThread ids above came from the X-Worker-Thread-Id response header.\n"
