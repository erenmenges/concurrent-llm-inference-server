#!/usr/bin/env bash
set -euo pipefail
./build/bench_http 127.0.0.1 8080 1 1 16 > /dev/null # warmup for server

for k in 1 2 4 8 16 32; do
  ./build/bench_http 127.0.0.1 8080 "$k" $((128 / k)) 128
done | tee "bench_$1.txt" # $1 writes the second arg when we run the script, i use it for static/continuous