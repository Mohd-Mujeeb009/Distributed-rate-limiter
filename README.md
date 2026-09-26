# Distributed Rate Limiter

[![CI](https://github.com/Mohd-Mujeeb009/Distributed-rate-limiter/actions/workflows/ci.yml/badge.svg)](https://github.com/Mohd-Mujeeb009/Distributed-rate-limiter/actions/workflows/ci.yml)
[![CodeQL](https://github.com/Mohd-Mujeeb009/Distributed-rate-limiter/actions/workflows/codeql.yml/badge.svg)](https://github.com/Mohd-Mujeeb009/Distributed-rate-limiter/actions/workflows/codeql.yml)

A C++20 portfolio service that demonstrates rate-limiting algorithms, concurrency safety, dependency injection, atomic Redis Lua design, metrics, containers, and CI.

## Run

```bash
docker compose up --build
curl -H 'Content-Type: application/json' -d '{"key":"user:42","rule":"api_default","cost":1}' http://localhost:8081/v1/check
```

Endpoints: `POST /v1/check`, `GET /v1/rules`, `GET /healthz`, and `GET /metrics`. Grafana is exposed at `:3000` and Prometheus at `:9090`.

## Build and test locally

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Algorithms

| Algorithm | Accuracy | Space/key | Best use |
|---|---|---:|---|
| Fixed window | Can burst at edges | O(1) | Cheap coarse limits |
| Sliding log | Exact | O(limit) | Strict low-volume limits |
| Sliding counter | Approximate weighted window | O(1) | Smooth high-volume limits |
| Token bucket | Exact refill; controlled bursts | O(1) | General API traffic |

The Strategy interface keeps algorithms replaceable; the factory centralizes construction; `IClock` makes rollover/refill tests deterministic. Redis-side time avoids clock skew, and one Lua invocation is the atomicity boundary across nodes. A fail-open policy favors availability; fail-closed favors protection.

## Current scope

The runnable binary uses the thread-safe in-memory strategies. Atomic Redis scripts and the three-node deployment topology are included as the integration seam; wiring `RedisStore` with `EVALSHA`, plus live distributed/chaos benchmarks, is the next production increment. Benchmark tables remain `TBD` until measured—this repo never claims invented performance.
