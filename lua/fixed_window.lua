local current = redis.call('INCRBY', KEYS[1], ARGV[1])
if current == tonumber(ARGV[1]) then redis.call('PEXPIRE', KEYS[1], ARGV[3]) end
local ttl = redis.call('PTTL', KEYS[1])
return {current <= tonumber(ARGV[2]) and 1 or 0, math.max(0, tonumber(ARGV[2]) - current), ttl}
