local ljd = require('ljd')
local inspect = require('inspect.inspect')
local function P(...)
  local n, args = select('#', ...), {...}
  for i = 1, n do
    args[i] = inspect(args[i])
  end
  print(unpack(args, 1, n))
end

local d = ljd.new(function()
  local function lmao(value)
    for i = 1, value do
      print(i)
    end
  end

  local x = 3
  print('A')
  lmao(x)
  print('B')
end)

local function get_locals()
  local level = 0
  while true do
    local locals = d:locals(level)
    if not locals then return end

    for i, name in ipairs(locals) do
      local k, v = debug.getlocal(d.coro, 0, i)
      if not k then break end
      if name ~= '(*temporary)' then
        print(('#%d.%d %-16s = %s'):format(level, i, name, v))
      end
    end

    level = level + 1
  end
end

local function state()
  local info = debug.getinfo(d.coro, 0, 'Slf')
  if not info then
    return print((':: %-16s -\n'):format(d.status))
  end
  print((':: %-16s %s:%d'):format(d.status, info.source, d.currentline))

  get_locals()

  -- local lines = {}
  -- for line in pairs(ljd.getlines(info.func)) do
  --   lines[#lines+1] = line
  -- end
  -- table.sort(lines)
  -- print(("lines: %d => %d: %s"):format(info.linedefined, info.lastlinedefined, table.concat(lines, ', ')))
  print()
end

for _ = 1, 100 do
  local res = d:step()
  state()
  if res == true or res == false then
    break
  end
end
