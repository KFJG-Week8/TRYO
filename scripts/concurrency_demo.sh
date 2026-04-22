#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-8080}"
COUNT="${2:-40}"
PARALLEL="${3:-12}"
BASE_URL="http://127.0.0.1:${PORT}"
TMP_FILE="$(mktemp)"

export BASE_URL

cleanup() {
  rm -f "${TMP_FILE}"
}

trap cleanup EXIT

echo "Health:"
curl -s "${BASE_URL}/health"
echo
echo

echo "Sending ${COUNT} INSERT requests with parallelism ${PARALLEL}..."
seq 1 "${COUNT}" | xargs -P "${PARALLEL}" -I{} sh -c '
  i="$1"
  id=$((10000 + i))
  age=$((20 + (i % 10)))
  curl -s -X POST "${BASE_URL}/query" \
    -H "Content-Type: text/plain" \
    --data "INSERT INTO users VALUES (${id}, '\''parallel${i}'\'', ${age});"
  printf "\n"
' _ {} > "${TMP_FILE}"

OK_COUNT="$(awk '{
  line = $0
  while (match(line, /"ok":true/)) {
    count++
    line = substr(line, RSTART + RLENGTH)
  }
}
END {
  print count + 0
}' "${TMP_FILE}")"

echo "Successful responses: ${OK_COUNT} / ${COUNT}"
echo
echo "Sample response:"
head -n 1 "${TMP_FILE}"
echo
echo
echo "Done. Check the server terminal for multiple [thread=...] values."
