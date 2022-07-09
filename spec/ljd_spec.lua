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

    eq(D.currentline, -1)
    eq({ D:step() }, { 0 })
    eq(D.currentline, line + 1)
    eq({ D:step() }, { 0 })
    eq(D.currentline, line + 2)
    eq({ D:step() }, { 0 })
    eq(D.currentline, line + 3)
    eq({ D:step() }, { true, 1, 2 })
  end)

  it('can next() through function', function()
    local function f()
      local a = 1
      local b = 2
      return a, b
    end

    local line = getline(f)
    local D = ljd.new(f)

    eq(D.currentline, -1)
    eq({ D:step() }, { 0 })
    eq(D.currentline, line + 1)
    eq({ D:next() }, { 0 })
    eq(D.currentline, line + 2)
    eq({ D:next() }, { 0 })
    eq(D.currentline, line + 3)
    eq({ D:next() }, { true, 1, 2 })
  end)
end)
