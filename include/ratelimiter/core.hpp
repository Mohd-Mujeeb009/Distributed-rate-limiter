#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ratelimiter {

struct Decision {
  bool allowed{};
  std::uint64_t limit{};
  std::uint64_t remaining{};
  std::uint64_t reset_ms{};
  std::uint64_t retry_after_ms{};
};

enum class Algorithm { fixed_window, sliding_window_log, sliding_window_counter, token_bucket };

struct Rule {
  std::string name;
  Algorithm algorithm{Algorithm::token_bucket};
  std::uint64_t limit{1};
  std::uint64_t window_ms{1000};
  double refill_per_sec{1.0};
};

class IClock {
 public:
  virtual ~IClock() = default;
  [[nodiscard]] virtual std::uint64_t now_ms() const = 0;
};

class SystemClock final : public IClock {
 public:
  [[nodiscard]] std::uint64_t now_ms() const override {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
  }
};

class FakeClock final : public IClock {
 public:
  explicit FakeClock(std::uint64_t now = 0) : now_(now) {}
  [[nodiscard]] std::uint64_t now_ms() const override { return now_; }
  void advance(std::uint64_t milliseconds) { now_ += milliseconds; }
 private:
  std::uint64_t now_;
};

class IRateLimiter {
 public:
  virtual ~IRateLimiter() = default;
  virtual Decision check(const std::string& key, std::uint64_t cost = 1) = 0;
};

class FixedWindow final : public IRateLimiter {
 public:
  FixedWindow(Rule rule, std::shared_ptr<IClock> clock) : rule_(std::move(rule)), clock_(std::move(clock)) {}
  Decision check(const std::string& key, std::uint64_t cost = 1) override {
    std::scoped_lock lock(mu_);
    const auto now = clock_->now_ms();
    const auto window = now / rule_.window_ms;
    auto& state = states_[key];
    if (state.first != window) state = {window, 0};
    const bool allowed = state.second + cost <= rule_.limit;
    if (allowed) state.second += cost;
    const auto reset = rule_.window_ms - now % rule_.window_ms;
    return {allowed, rule_.limit, rule_.limit - state.second, reset, allowed ? 0 : reset};
  }
 private:
  Rule rule_;
  std::shared_ptr<IClock> clock_;
  std::mutex mu_;
  std::unordered_map<std::string, std::pair<std::uint64_t, std::uint64_t>> states_;
};

class SlidingWindowLog final : public IRateLimiter {
 public:
  SlidingWindowLog(Rule rule, std::shared_ptr<IClock> clock) : rule_(std::move(rule)), clock_(std::move(clock)) {}
  Decision check(const std::string& key, std::uint64_t cost = 1) override {
    std::scoped_lock lock(mu_);
    const auto now = clock_->now_ms();
    auto& log = states_[key];
    log.erase(std::remove_if(log.begin(), log.end(), [&](auto t) { return t + rule_.window_ms <= now; }), log.end());
    const bool allowed = log.size() + cost <= rule_.limit;
    if (allowed) for (std::uint64_t i = 0; i < cost; ++i) log.push_back(now);
    const auto reset = log.empty() ? 0 : std::max<std::uint64_t>(1, log.front() + rule_.window_ms - now);
    return {allowed, rule_.limit, rule_.limit - std::min<std::uint64_t>(rule_.limit, log.size()), reset, allowed ? 0 : reset};
  }
 private:
  Rule rule_;
  std::shared_ptr<IClock> clock_;
  std::mutex mu_;
  std::unordered_map<std::string, std::vector<std::uint64_t>> states_;
};

class SlidingWindowCounter final : public IRateLimiter {
 public:
  SlidingWindowCounter(Rule rule, std::shared_ptr<IClock> clock) : rule_(std::move(rule)), clock_(std::move(clock)) {}
  Decision check(const std::string& key, std::uint64_t cost = 1) override {
    std::scoped_lock lock(mu_);
    const auto now = clock_->now_ms();
    const auto window = now / rule_.window_ms;
    auto& s = states_[key];
    if (s.window != window) {
      s.previous = s.window + 1 == window ? s.current : 0;
      s.current = 0;
      s.window = window;
    }
    const double elapsed = static_cast<double>(now % rule_.window_ms) / rule_.window_ms;
    const double estimated = s.previous * (1.0 - elapsed) + s.current;
    const bool allowed = estimated + cost <= rule_.limit;
    if (allowed) s.current += cost;
    const auto used = std::min<std::uint64_t>(rule_.limit, static_cast<std::uint64_t>(std::ceil(estimated + (allowed ? cost : 0))));
    const auto reset = rule_.window_ms - now % rule_.window_ms;
    return {allowed, rule_.limit, rule_.limit - used, reset, allowed ? 0 : reset};
  }
 private:
  struct State { std::uint64_t window{}, current{}, previous{}; };
  Rule rule_;
  std::shared_ptr<IClock> clock_;
  std::mutex mu_;
  std::unordered_map<std::string, State> states_;
};

class TokenBucket final : public IRateLimiter {
 public:
  TokenBucket(Rule rule, std::shared_ptr<IClock> clock) : rule_(std::move(rule)), clock_(std::move(clock)) {}
  Decision check(const std::string& key, std::uint64_t cost = 1) override {
    std::scoped_lock lock(mu_);
    const auto now = clock_->now_ms();
    auto [it, inserted] = states_.try_emplace(key, State{static_cast<double>(rule_.limit), now});
    auto& s = it->second;
    if (!inserted) {
      s.tokens = std::min<double>(rule_.limit, s.tokens + (now - s.updated_ms) * rule_.refill_per_sec / 1000.0);
      s.updated_ms = now;
    }
    const bool allowed = s.tokens >= cost;
    if (allowed) s.tokens -= cost;
    const auto remaining = static_cast<std::uint64_t>(std::floor(s.tokens));
    const auto needed = allowed ? rule_.limit - s.tokens : std::max(0.0, cost - s.tokens);
    const auto wait = rule_.refill_per_sec > 0 ? static_cast<std::uint64_t>(std::ceil(needed * 1000.0 / rule_.refill_per_sec)) : rule_.window_ms;
    return {allowed, rule_.limit, remaining, wait, allowed ? 0 : wait};
  }
 private:
  struct State { double tokens; std::uint64_t updated_ms; };
  Rule rule_;
  std::shared_ptr<IClock> clock_;
  std::mutex mu_;
  std::unordered_map<std::string, State> states_;
};

inline std::unique_ptr<IRateLimiter> make_limiter(const Rule& rule, std::shared_ptr<IClock> clock) {
  switch (rule.algorithm) {
    case Algorithm::fixed_window: return std::make_unique<FixedWindow>(rule, std::move(clock));
    case Algorithm::sliding_window_log: return std::make_unique<SlidingWindowLog>(rule, std::move(clock));
    case Algorithm::sliding_window_counter: return std::make_unique<SlidingWindowCounter>(rule, std::move(clock));
    case Algorithm::token_bucket: return std::make_unique<TokenBucket>(rule, std::move(clock));
  }
  throw std::invalid_argument("unknown algorithm");
}

}  // namespace ratelimiter
