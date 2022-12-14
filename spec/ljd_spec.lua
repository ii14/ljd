local ljd = require('ljd')

local eq = assert.is_same

local cache = {}
local function L(f)
  local line = cache[f]
  if not line then
    local info = assert(debug.getinfo(f, 'S'), 'debug.getinfo')
    line = assert(info.linedefined, 'debug.getinfo linedefined')
    cache[f] = line
  end
  return line
end

local function filter_locals(t)
  if t == nil then
    return nil
  end
  for k, v in pairs(t) do
    if v == '(*temporary)' then
      t[k] = nil
    end
  end
  return t
end

describe('ljd', function()
  describe('step()', function()
    it('can step through function', function()
      local function f()
        local a = 1
        local b = 2
        return a, b
      end

      local D = ljd.new(f)

      eq(-1, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 1, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 2, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 3, D.currentline)
      eq({true, 1, 2}, {D:step()})
      eq(-1, D.currentline)
    end)

    it('can step into nested function', function()
      local function f2()
        local x = 2
        return x
      end

      local function f()
        local a = 1
        local b = f2()
        return a, b
      end

      local D = ljd.new(f)

      eq(-1, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 1, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 2, D.currentline)
      eq({0}, {D:step()})
      eq(L(f2) + 1, D.currentline)
      eq({0}, {D:step()})
      eq(L(f2) + 2, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 3, D.currentline)
      eq({true, 1, 2}, {D:step()})
      eq(-1, D.currentline)
    end)
  end)

  describe('next()', function()
    it('can step through function', function()
      local function f()
        local a = 1
        local b = 2
        return a, b
      end

      local D = ljd.new(f)

      eq(-1, D.currentline)
      eq({0}, {D:next()})
      eq(L(f) + 1, D.currentline)
      eq({0}, {D:next()})
      eq(L(f) + 2, D.currentline)
      eq({0}, {D:next()})
      eq(L(f) + 3, D.currentline)
      eq({true, 1, 2}, {D:next()})
      eq(-1, D.currentline)
    end)

    it('can skip nested function', function()
      local function f2()
        local x = 2
        return x
      end

      local function f()
        local a = 1
        local b = f2()
        return a, b
      end

      local D = ljd.new(f)

      eq(-1, D.currentline)
      eq({0}, {D:next()})
      eq(L(f) + 1, D.currentline)
      eq({0}, {D:next()})
      eq(L(f) + 2, D.currentline)
      eq({0}, {D:next()})
      eq(L(f) + 3, D.currentline)
      eq({true, 1, 2}, {D:next()})
      eq(-1, D.currentline)
    end)
  end)

  describe('finish()', function()
    it('can run function from start to end', function()
      local function f()
        local a = 1
        local b = 2
        return a, b
      end

      local D = ljd.new(f)

      eq(-1, D.currentline)
      eq({true, 1, 2}, {D:finish()})
      eq(-1, D.currentline)
    end)

    it('can resume execution', function()
      local function f()
        local a = 1
        local b = 2
        return a, b
      end

      local D = ljd.new(f)

      eq(-1, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 1, D.currentline)
      eq({true, 1, 2}, {D:finish()})
      eq(-1, D.currentline)
    end)

    it('can step out of nested function', function()
      local function f2()
        local x = 1
        return x
      end

      local function f()
        local a = f2()
        local b = 2
        return a, b
      end

      local D = ljd.new(f)

      eq(-1, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 1, D.currentline)
      eq({0}, {D:step()})
      eq(L(f2) + 1, D.currentline)
      eq({0}, {D:finish()})
      eq(L(f) + 2, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 3, D.currentline)
      eq({true, 1, 2}, {D:step()})
      eq(-1, D.currentline)
    end)

    it('steps out of just one function', function()
      local function f3()
        local y = 0
        y = y + 1
        return y
      end

      local function f2()
        local x = f3()
        return x
      end

      local function f()
        local a = f2()
        local b = 2
        return a, b
      end

      local D = ljd.new(f)

      eq(-1, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 1, D.currentline)
      eq({0}, {D:step()})
      eq(L(f2) + 1, D.currentline)
      eq({0}, {D:step()})
      eq(L(f3) + 1, D.currentline)
      eq({0}, {D:finish()})
      eq(L(f2) + 2, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 2, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 3, D.currentline)
      eq({true, 1, 2}, {D:step()})
      eq(-1, D.currentline)
    end)
  end)

  describe('continue()', function()
    it('can run function from start to end', function()
      local function f()
        local a = 1
        local b = 2
        return a, b
      end

      local D = ljd.new(f)

      eq(-1, D.currentline)
      eq({true, 1, 2}, {D:continue()})
      eq(-1, D.currentline)
    end)

    it('can resume execution', function()
      local function f()
        local a = 1
        local b = 2
        return a, b
      end

      local D = ljd.new(f)

      eq(-1, D.currentline)
      eq({0}, {D:step()})
      eq(L(f) + 1, D.currentline)
      eq({true, 1, 2}, {D:continue()})
      eq(-1, D.currentline)
    end)
  end)

  it('can pass arguments to the function', function()
    local function f(a, b)
      a = a * 2
      b = b * 2
      return a, b
    end

    local D = ljd.new(f, 1, 2)

    eq({ true, 2, 4 }, { D:continue() })
  end)

  describe('locals()', function()
    it('can inspect local variables', function()
      local function f()
        local a = 1
        local b = 2
        return a, b
      end

      local D = ljd.new(f)

      eq({0}, {D:step()})
      eq({}, filter_locals(D:locals(0)))
      eq({0}, {D:step()})
      eq({'a'}, filter_locals(D:locals(0)))
      eq({0}, {D:step()})
      eq({'a', 'b'}, filter_locals(D:locals(0)))
      eq({true, 1, 2}, {D:step()})
      eq(-1, D.currentline)
    end)

    it('works in nested functions', function()
      local function f3()
        local y = 2
        return y
      end

      local function f2()
        local x = f3()
        return x
      end

      local function f()
        local a = 1
        local b = f2()
        return a, b
      end

      local D = ljd.new(f)

      eq({0}, {D:step()})
      eq({}, filter_locals(D:locals(0)))
      eq(nil, filter_locals(D:locals(1)))

      eq({0}, {D:step()})
      eq({'a'}, filter_locals(D:locals(0)))
      eq(nil, filter_locals(D:locals(1)))

      eq({0}, {D:step()})
      eq({}, filter_locals(D:locals(0)))
      eq({'a'}, filter_locals(D:locals(1)))
      eq(nil, filter_locals(D:locals(2)))

      eq({0}, {D:step()})
      eq({}, filter_locals(D:locals(0)))
      eq({}, filter_locals(D:locals(1)))
      eq({'a'}, filter_locals(D:locals(2)))
      eq(nil, filter_locals(D:locals(3)))

      eq({0}, {D:step()})
      eq({'y'}, filter_locals(D:locals(0)))
      eq({}, filter_locals(D:locals(1)))
      eq({'a'}, filter_locals(D:locals(2)))
      eq(nil, filter_locals(D:locals(3)))

      eq({0}, {D:step()})
      eq({'x'}, filter_locals(D:locals(0)))
      eq({'a'}, filter_locals(D:locals(1)))
      eq(nil, filter_locals(D:locals(2)))

      eq({0}, {D:step()})
      eq({'a', 'b'}, filter_locals(D:locals(0)))
      eq(nil, filter_locals(D:locals(1)))

      eq({true, 1, 2}, {D:step()})
      eq(-1, D.currentline)
    end)
  end)

  -- TODO: test breakpoints
end)
