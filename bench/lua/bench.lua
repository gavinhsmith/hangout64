-- Game-like workloads for measuring Lua on each platform. bench.c compiles this chunk (timing
-- the compile), runs it once to set up, then calls each workload's tick function repeatedly.
-- Integer maths only (// not /), as experiences would use.

local W = {}

-- 64 scripted parts sliding around and bouncing off the scene bounds.
local movers = {}
for i = 1, 64 do
  movers[i] = {x = i * 137 % 20000 - 10000, z = i * 311 % 20000 - 10000,
               vx = i % 7 * 10 - 30, vz = i % 5 * 10 - 20}
end
W.movers = function()
  for i = 1, #movers do
    local m = movers[i]
    m.x = m.x + m.vx
    m.z = m.z + m.vz
    if m.x < -10000 or m.x > 10000 then m.vx = -m.vx end
    if m.z < -10000 or m.z > 10000 then m.vz = -m.vz end
  end
end

-- Every pair of 32 boxes checked for overlap (496 pairs): a heavy, game-logic-style loop.
local boxes = {}
for i = 1, 32 do
  boxes[i] = {x = i * 523 % 8000, y = i * 97 % 3000, z = i * 829 % 8000, s = 300 + i * 13 % 400}
end
W.overlaps = function()
  local hits = 0
  for i = 1, #boxes - 1 do
    local a = boxes[i]
    for j = i + 1, #boxes do
      local b = boxes[j]
      local s = a.s + b.s
      if a.x - b.x < s and b.x - a.x < s and a.y - b.y < s and b.y - a.y < s
         and a.z - b.z < s and b.z - a.z < s then
        hits = hits + 1
      end
    end
  end
  return hits
end

-- 200 events dispatched through a listener table to closures: Roblox-style :connect callbacks.
local listeners, score = {}, 0
for i = 1, 8 do
  listeners[i] = function(who, amount) score = score + amount * i - who end
end
local function fire(who, amount)
  for i = 1, #listeners do listeners[i](who, amount) end
end
W.events = function()
  for k = 1, 25 do fire(k, k % 3) end
end

-- 10 UI strings rebuilt per tick: concatenation and string.format.
W.strings = function()
  local t = {}
  for i = 1, 10 do
    t[i] = "Player " .. i .. ": " .. string.format("%d pts (%d%%)", score % 100000 + i, i * 9)
  end
  return table.concat(t, "\n")
end

-- 50 short-lived tables per tick: garbage-collector pressure.
W.alloc = function()
  local keep
  for i = 1, 50 do keep = {i, i + 1, name = "tmp", x = i * 2} end
  return keep
end

-- Everything above in one tick: a busy but plausible experience.
W.mix = function()
  W.movers(); W.overlaps(); W.events(); W.strings(); W.alloc()
end

return W
