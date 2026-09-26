# Reproducible benchmark plan

Numbers are intentionally not fabricated. Run `docker compose up --build`, drive `/v1/check` with k6 at 10, 100, and 500 virtual users, and record machine CPU/RAM here.

| Algorithm | Accuracy | Memory/key | p50 | p99 | Max RPS (3 nodes) |
|---|---|---:|---:|---:|---:|
| Fixed window | Edge bursts | O(1) | TBD | TBD | TBD |
| Sliding log | Exact | O(limit) | TBD | TBD | TBD |
| Sliding counter | Approximate | O(1) | TBD | TBD | TBD |
| Token bucket | Exact; permits bursts | O(1) | TBD | TBD | TBD |
