-- Atomic token bucket. Redis TIME prevents skew between service instances.
local now_parts = redis.call('TIME')
local now = now_parts[1] * 1000 + math.floor(now_parts[2] / 1000)
local state = redis.call('HMGET', KEYS[1], 'tokens', 'updated')
local tokens = tonumber(state[1]) or tonumber(ARGV[1])
local updated = tonumber(state[2]) or now
tokens = math.min(tonumber(ARGV[1]), tokens + (now - updated) * tonumber(ARGV[2]) / 1000)
local cost = tonumber(ARGV[3])
local allowed = tokens >= cost
if allowed then tokens = tokens - cost end
redis.call('HSET', KEYS[1], 'tokens', tokens, 'updated', now)
redis.call('PEXPIRE', KEYS[1], ARGV[4])
return {allowed and 1 or 0, math.floor(tokens), now}
