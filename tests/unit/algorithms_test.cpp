#include <cassert>
#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include "ratelimiter/core.hpp"

using namespace ratelimiter;

int main() {
  auto clock = std::make_shared<FakeClock>();
  TokenBucket bucket({"bucket", Algorithm::token_bucket, 5, 1000, 1}, clock);
  for (int i = 0; i < 5; ++i) assert(bucket.check("alice").allowed);
  assert(!bucket.check("alice").allowed);
  clock->advance(1000);
  assert(bucket.check("alice").allowed);

  FixedWindow fixed({"fixed", Algorithm::fixed_window, 2, 1000, 0}, clock);
  assert(fixed.check("a").allowed && fixed.check("a").allowed && !fixed.check("a").allowed);
  clock->advance(1000);
  assert(fixed.check("a").allowed);

  SlidingWindowLog log({"log", Algorithm::sliding_window_log, 2, 1000, 0}, clock);
  assert(log.check("a").allowed && log.check("a").allowed && !log.check("a").allowed);
  clock->advance(1000);
  assert(log.check("a").allowed);

  auto concurrent_clock = std::make_shared<FakeClock>();
  TokenBucket concurrent({"c", Algorithm::token_bucket, 100, 1000, 0}, concurrent_clock);
  std::atomic<int> accepted{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < 16; ++t) threads.emplace_back([&] { for (int i = 0; i < 100; ++i) accepted += concurrent.check("shared").allowed; });
  for (auto& thread : threads) thread.join();
  assert(accepted == 100);
  std::cout << "all algorithm and concurrency tests passed\n";
}
