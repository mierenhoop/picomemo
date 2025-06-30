local lsqlite3 = require"lsqlite3"

local function NewDatabase(path, opts)
  opts = opts or {}
  local ESQLITE = "db: lsqlite3 not found"
  local EOPEN =  "db: could not open"
  local ECLOSED =  "db: can not perform on closed db"
  local lsqlite3 = assert(lsqlite3 or require"lsqlite3", ESQLITE)
  local intrans = false
  local db = assert(lsqlite3.open(path), EOPEN)
  db:busy_timeout(opts.timeout or 1000)
  local function dbok(ret)
    if ret ~= lsqlite3.OK then
      error(db:errmsg())
    end
  end
  local function prep(sql, ...)
    assert(db, ECLOSED)
    local stmt = db:prepare(sql)
    if not stmt then error(db:errmsg()) end
    dbok(stmt:bind_values(...))
    return stmt
  end
  local function urows(sql, ...)
    local stmt = prep(sql, ...)
    local closer = setmetatable({}, {
      __close = function() dbok(stmt:finalize()) end
    })
    return stmt:urows(), stmt, nil, closer
  end
  local function exec(sql)
    assert(db, ECLOSED)
    dbok(db:exec(sql))
  end
  local function close() if db then db:close() end db = nil end
  return setmetatable({
    ESQLITE = ESQLITE, EOPEN = EOPEN, ECLOSED = ECLOSED,
    exec = exec,
    urows = urows,
    urow = function(sql, ...)
      local iter, state, _, closer <close> = urows(sql, ...)
      return iter(state)
    end,
    transaction = function(f)
      if intrans then return f() end
      exec"BEGIN TRANSACTION;"
      intrans = true
      local ok, err = pcall(f)
      intrans = false
      if not ok then
        exec"ROLLBACK;"
        error(err)
      end
      exec"COMMIT;"
    end,
  }, { __close = close })
end

return NewDatabase
