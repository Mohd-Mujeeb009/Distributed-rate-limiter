#include <atomic>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>

#include <httplib.h>
#include <nlohmann/json.hpp>
#include "ratelimiter/core.hpp"

using json = nlohmann::json;
using namespace ratelimiter;

int main() {
  auto clock = std::make_shared<SystemClock>();
  std::unordered_map<std::string, Rule> rules{
      {"api_default", {"api_default", Algorithm::token_bucket, 100, 60000, 2}},
      {"login_strict", {"login_strict", Algorithm::sliding_window_log, 5, 60000, 0}},
      {"search", {"search", Algorithm::sliding_window_counter, 50, 10000, 0}},
      {"fixed_demo", {"fixed_demo", Algorithm::fixed_window, 5, 10000, 0}},
  };
  std::unordered_map<std::string, std::unique_ptr<IRateLimiter>> limiters;
  for (const auto& [name, rule] : rules) limiters[name] = make_limiter(rule, clock);
  std::atomic<std::uint64_t> allowed{0}, blocked{0};
  httplib::Server server;

  server.Get("/healthz", [](const auto&, auto& res) {
    res.set_content(R"({"status":"ok","store":"in-memory"})", "application/json");
  });
  server.Get("/v1/rules", [&](const auto&, auto& res) {
    json out = json::array();
    for (const auto& [name, rule] : rules) out.push_back({{"name", name}, {"limit", rule.limit}, {"window_ms", rule.window_ms}});
    res.set_content(out.dump(), "application/json");
  });
  server.Get("/metrics", [&](const auto&, auto& res) {
    res.set_content("# TYPE rate_limiter_requests_total counter\nrate_limiter_requests_total{decision=\"allowed\"} " +
                    std::to_string(allowed.load()) + "\nrate_limiter_requests_total{decision=\"blocked\"} " +
                    std::to_string(blocked.load()) + "\n", "text/plain; version=0.0.4");
  });
  server.Post("/v1/check", [&](const auto& req, auto& res) {
    try {
      const auto body = json::parse(req.body);
      const auto key = body.at("key").get<std::string>();
      const auto rule_name = body.at("rule").get<std::string>();
      const auto cost = body.value("cost", 1ULL);
      if (!limiters.contains(rule_name) || key.empty() || cost == 0) throw std::invalid_argument("invalid key, rule, or cost");
      const auto d = limiters.at(rule_name)->check(key, cost);
      (d.allowed ? allowed : blocked).fetch_add(1);
      res.status = d.allowed ? 200 : 429;
      res.set_header("X-RateLimit-Limit", std::to_string(d.limit));
      res.set_header("X-RateLimit-Remaining", std::to_string(d.remaining));
      if (!d.allowed) res.set_header("Retry-After", std::to_string((d.retry_after_ms + 999) / 1000));
      json response;
      response["allowed"] = d.allowed;
      response["limit"] = d.limit;
      response["remaining"] = d.remaining;
      response["reset_ms"] = d.reset_ms;
      response["retry_after_ms"] = d.retry_after_ms;
      res.set_content(response.dump(), "application/json");
    } catch (const std::exception& e) {
      res.status = 400;
      res.set_content(json{{"error", e.what()}}.dump(), "application/json");
    }
  });
  const int port = std::getenv("PORT") ? std::stoi(std::getenv("PORT")) : 8081;
  server.listen("0.0.0.0", port);
}
