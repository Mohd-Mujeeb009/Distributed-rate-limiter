# Architecture

Requests reach one of three stateless C++20 services through Nginx. The HTTP layer validates input and delegates to an `IRateLimiter` strategy. Algorithms are isolated from time through `IClock`; tests use `FakeClock` and production uses `SystemClock`. The in-process implementation uses a mutex per limiter, while the supplied Lua scripts show the atomic Redis transaction boundary used by the distributed store.

Redis scripts use Redis `TIME`, perform the entire read-modify-write atomically, and attach TTLs. Production hardening should load scripts once and use `EVALSHA`, retrying with `SCRIPT LOAD` after `NOSCRIPT`.
