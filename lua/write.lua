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

local p = table.insert

function EncodeXml(t, b, indent)
  if type(t) ~= "table" then
    local esc = Escape(tostring(t))
    if indent then
      p(b,indent)
      esc = esc:gsub("\n", "\n"..indent).."\n"
    end
    p(b,esc)
    return
  end
  local name = t[0]
  assert(type(name) == "string")
  assert(name:match"^[:_%a][:_%w%-%.]*$")
  if indent then p(b,indent) end
  p(b,"<") p(b,name)
  local attrs = {}
  -- sort
  for k in pairs(t) do
    if type(k) ~= "number" then
      assert(type(k) == "string")
      assert(k:match"^[:_%a][:_%w%-%.]*$")
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
    local in2
    if indent then
      in2 = indent.."  "
      p(b,"\n")
    end
    for i = 1, #t do
      EncodeXml(t[i], b, in2)
    end
    if indent then p(b,indent) end
    p(b,"</") p(b,name) p(b,">")
  else
    p(b,"/>")
  end
  if indent then p(b,"\n") end
end
