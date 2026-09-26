#!/usr/bin/env sh
set -eu
for i in 1 2 3 4 5 6 7; do
  curl -s -o /dev/null -w '%{http_code}\n' -H 'Content-Type: application/json' \
    -d '{"key":"demo","rule":"fixed_demo","cost":1}' http://localhost:8081/v1/check
done
