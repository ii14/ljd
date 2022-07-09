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
      eq({ 0 }, { D:step() })
      eq(L(f) + 1, D.currentline)
      eq({ 0 }, { D:step() })
      eq(L(f) + 2, D.currentline)
      eq({ 0 }, { D:step() })
      eq(L(f) + 3, D.currentline)
      eq({ true, 1, 2 }, { D:step() })
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
      eq({ 0 }, { D:next() })
      eq(L(f) + 1, D.currentline)
      eq({ 0 }, { D:next() })
      eq(L(f) + 2, D.currentline)
      eq({ 0 }, { D:next() })
      eq(L(f) + 3, D.currentline)
      eq({ true, 1, 2 }, { D:next() })
      eq(-1, D.currentline)
    end)
  end)

  describe('finish()', function()
    it('can resume execution', function()
      local function f()
        local a = 1
        local b = 2
        return a, b
      end

      local D = ljd.new(f)

      eq(-1, D.currentline)
      eq({ 0 }, { D:step() })
      eq(L(f) + 1, D.currentline)
      eq({ true, 1, 2 }, { D:finish() })
      eq(-1, D.currentline)
    end)
  end)

  describe('continue()', function()
    it('can resume execution', function()
      local function f()
        local a = 1
        local b = 2
        return a, b
      end

      local D = ljd.new(f)

      eq(-1, D.currentline)
      eq({ 0 }, { D:step() })
      eq(L(f) + 1, D.currentline)
      eq({ true, 1, 2 }, { D:continue() })
      eq(-1, D.currentline)
    end)
  end)
end)
