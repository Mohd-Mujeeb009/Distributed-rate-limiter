FROM ubuntu:26.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential cmake ca-certificates curl && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j2 && ctest --test-dir build --output-on-failure

FROM ubuntu:26.04
RUN useradd --system --uid 10001 limiter
COPY --from=build /src/build/rate-limiter /usr/local/bin/rate-limiter
USER limiter
EXPOSE 8081
ENTRYPOINT ["rate-limiter"]
