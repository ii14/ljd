local ljd = require('ljd')

local eq = assert.is_same

local function getline(f)
  local info = assert(debug.getinfo(f, 'S'), 'debug.getinfo')
  return assert(info.linedefined, 'debug.getinfo linedefined')
end

describe('ljd', function()
  describe('step()', function()
    it('can step through function', function()
      local function f()
        local a = 1
        local b = 2
        return a, b
      end

      local D, L = ljd.new(f), getline(f)

      eq(-1, D.currentline)
      eq({ 0 }, { D:step() })
      eq(L + 1, D.currentline)
      eq({ 0 }, { D:step() })
      eq(L + 2, D.currentline)
      eq({ 0 }, { D:step() })
      eq(L + 3, D.currentline)
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

      local D, L = ljd.new(f), getline(f)

      eq(-1, D.currentline)
      eq({ 0 }, { D:next() })
      eq(L + 1, D.currentline)
      eq({ 0 }, { D:next() })
      eq(L + 2, D.currentline)
      eq({ 0 }, { D:next() })
      eq(L + 3, D.currentline)
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

      local D, L = ljd.new(f), getline(f)

      eq(-1, D.currentline)
      eq({ 0 }, { D:step() })
      eq(L + 1, D.currentline)
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

      local D, L = ljd.new(f), getline(f)

      eq(-1, D.currentline)
      eq({ 0 }, { D:step() })
      eq(L + 1, D.currentline)
      eq({ true, 1, 2 }, { D:continue() })
      eq(-1, D.currentline)
    end)
  end)
end)
