local ljd = require('ljd')

local eq = assert.is_same

local function getline(f)
  return assert(assert(debug.getinfo(f, 'S')).linedefined)
end

describe('ljd', function()
  it('can step() through function', function()
    local function f()
      local a = 1
      local b = 2
      return a, b
    end

    local line = getline(f)
    local D = ljd.new(f)

    eq(-1, D.currentline)
    eq({ 0 }, { D:step() })
    eq(line + 1, D.currentline)
    eq({ 0 }, { D:step() })
    eq(line + 2, D.currentline)
    eq({ 0 }, { D:step() })
    eq(line + 3, D.currentline)
    eq({ true, 1, 2 }, { D:step() })
    eq(line + 3, D.currentline)
  end)

  it('can next() through function', function()
    local function f()
      local a = 1
      local b = 2
      return a, b
    end

    local line = getline(f)
    local D = ljd.new(f)

    eq(-1, D.currentline)
    eq({ 0 }, { D:step() })
    eq(line + 1, D.currentline)
    eq({ 0 }, { D:next() })
    eq(line + 2, D.currentline)
    eq({ 0 }, { D:next() })
    eq(line + 3, D.currentline)
    eq({ true, 1, 2 }, { D:next() })
    eq(line + 3, D.currentline)
  end)

  it('can finish()', function()
    local function f()
      local a = 1
      local b = 2
      return a, b
    end

    local line = getline(f)
    local D = ljd.new(f)

    eq(-1, D.currentline)
    eq({ 0 }, { D:step() })
    eq(line + 1, D.currentline)
    eq({ true, 1, 2 }, { D:finish() })
    eq(line + 3, D.currentline)
  end)
end)
