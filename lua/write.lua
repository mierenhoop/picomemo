local buffer = {}

-- TODO: should we have different escaping for content and 
local function Escape(s)
  return (s:gsub("[&'\"<>]", {
    ["&"] = "&amp;",
    ["'"] = "&apos;",
    ['"'] = "&quot;",
    ["<"] = "&lt;",
    [">"] = "&gt;",
  }))
end

function Write(fmt, ...)
  local i = 1
  local args = {...}
  buffer[#buffer+1] = fmt:gsub("%%.", function(s)
    if s == "%%" then return "%" end
    local arg = assert(args[i])
    i=i+1
    if s == "%s" then return Escape(arg) end
    if s == "%b" then return EncodeBase64(arg) end
    if s == "%d" then return tostring(arg) end -- TODO: decimals?
    error"format specifier not found"
  end)
end

function Flush()
  Send(table.concat(buffer))
  buffer = {}
end

local p = table.insert

function EncodeXml(t, b)
  if type(t) ~= "table" then
    p(b,Escape(tostring(t)))
    return
  end
  local name = t[0]
  assert(type(name) == "string")
  assert(name:match"^[:%a][:%w%-%.]*$")
  p(b,"<") p(b,name)
  local attrs = {}
  -- sort
  for k in pairs(t) do
    if type(k) ~= "number" then
      assert(type(k) == "string")
      assert(k:match"^[:%a][:%w%-%.]*$")
      attrs[#attrs+1] = k
    end
  end
  table.sort(attrs)
  for i = 1, #attrs do
    p(b," ") p(b,attrs[i]) p(b,"=\"")
    p(b,Escape(tostring(t[attrs[i]]))) p(b,"\"")
  end
  if #t > 0 then
    p(b,">")
    for i = 1, #t do
      EncodeXml(t[i], b)
    end
    p(b,"</") p(b,name) p(b,">")
  else
    p(b,"/>")
  end
end

local b = {}
EncodeXml({ [0]="iq",
  id = "randomid",
  type = "get",
  {[0]="bind", xmlns="bind:'<>",
    {[0]="jid", "admin@localhost/resource" }
  },
}, b)
print(table.concat(b))
