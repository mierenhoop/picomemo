-- TODO: for more efficiency, the parse functions can do string.find
-- instead of checking each char, this will be more performant in most
-- use cases (large chunks, often multiple stanzas sent at once)

-- TODO: FFEF, accepted unicode ranges in Peek()

local function NewParser()
  local inputs = {}
  local pos
  local savei, savepos

  local function WaitForData()
    while #inputs == 0 or pos > #inputs[#inputs] do
      local input = coroutine.yield()
      if type(input) == "string" and #input > 0 then
        if not savei then
          if #input > 0 then inputs = {} end
          inputs[1] = input
        else
          inputs[#inputs+1] = input
        end
        pos = 1
        return
      end
    end
  end

  local function Save()
    assert(#inputs > 0 and pos)
    savei, savepos = #inputs, pos
  end

  local function Unsave()
    savei, savepos = nil, nil
  end

  local function GetSaved()
    assert(savei and savepos)
    local s
    if savei == #inputs then
      s = inputs[savei]:sub(savepos, pos-1)
    else
      s = {inputs[savei]:sub(savepos)}
      for i = savei+1, #inputs-1 do
        s[#s+1] = inputs[i]
      end
      s[#s+1] = inputs[#inputs]:sub(1, pos-1)
      s = table.concat(s)
    end
    Unsave()
    return s
  end

  local function Peek()
    WaitForData()
    assert(#inputs > 0 and pos)
    return inputs[#inputs]:sub(pos, pos)
  end

  local function Try(patt)
    local c = Peek()
    if c:match(patt) then
      pos = pos + 1
      return c
    end
  end

  local Wpatt = "[\x20\x09\x0d\x0a]"

  local function TryW()
    local s = Try(Wpatt)
    if s then
      repeat until not Try(Wpatt)
    end
    return s
  end

  local function Error(err)
    error("error: expected " .. err .. " at char " .. pos)
  end

  local function Expect(patt, err)
    local c = Try(patt)
    if not c then Error(err or patt) end
    return c
  end

  local function ExpectS(s, err)
    for i = 1, #s do
      Expect(s:sub(i, i), err or s)
    end
  end

  local function ParseW()
    repeat until not TryW()
  end

  local function ExpectW()
    Expect(Wpatt, "whitespace")
    ParseW()
  end

  -- Returns XML Name as string or nil
  local function ParseName()
    Save()
    -- TODO: verify UTF-8
    if Try("[:_%a\x80-\xff]") then
      repeat until not Try("[:_%w%-%.\x80-\xff]")
      return GetSaved()
    end
    Unsave()
  end

  local function ReplaceEntities(s)
    return (s:gsub("&([^;]*;?)", function(ent)
      local rep = ({["amp;"]="&",["lt;"]="<",["gt;"]=">",["apos;"]="'",["quot;"]="\""})[ent]
      if not rep then error"entity" end
      return rep
    end))
  end

  local function ParseAttributeValue()
    local q = Expect("['\"]", "' or \"")
    local notq = q == "'" and "[^']" or "[^\"]"
    Save()
    repeat until not Try(notq)
    local s = GetSaved()
    s = ReplaceEntities(s)
    Expect(q)
    return s
  end

  -- Parse from <elem (excluding space) until expected / or >
  local function ParseAttributes()
    local attrs = {}
    while TryW() do
      local key = ParseName()
      if not key then break end
      Expect"="
      local val = ParseAttributeValue()
      attrs[key] = val
    end
    return attrs
  end

  local function ParseCdata()
      Expect("%[", "[") ExpectS"CDATA" Expect("%[", "[")
      Save()
      while true do
        -- TODO: xml chars
        Try"[^%]]"
        local i=0
        while Try"%]" do
          i=i+1
        end
        if i >= 2 and Try">" then
          -- TODO: right? maybe have argument to GetSaved() for end offset
          return GetSaved():sub(1, -3)
        end
      end
  end

  -- Parse after opening <
  -- TODO: not have recursion?
  local function ParseElement()
    local name = ParseName()
    if not name then Error("opening tag") end
    local attrs = ParseAttributes()
    attrs[0] = name
    if Try"/" then
      Expect">"
      return attrs
    end
    Expect">"
    while true do
      -- TODO: do we want to omit the whitespace before possible content?
      ParseW()
      Save()
      -- TODO: xml chars
      if Try"[^<]" then
        while Try"[^<]" do
        end
        local content = GetSaved()
        -- TODO: strip whitespace?
        content = ReplaceEntities(content:gsub(Wpatt.."*$", ""))
        attrs[#attrs+1] = content
      else
        Unsave()
      end
      Expect"<"
      if Try"!" then
        -- TODO: merge CDATA with other content in same slot
        attrs[#attrs+1] = ParseCdata()
      else
        if Try"/" then
          if ParseName() ~= name then Error("same closing tag as opening tag") end
          ParseW()
          Expect">"
          break
        end
        attrs[#attrs+1] = ParseElement()
      end
    end
    return attrs
  end

  local r = coroutine.wrap(function()
    local s = TryW()
    Expect"<"
    if not s and Try"?" then
      ExpectS"xml"
      ExpectW()
      ExpectS"version="
      -- TODO: match"^[uU][tT][fF]%-8$" for XMPP
      if not ParseAttributeValue():match"^1%.%d+$" then
        Error("correct xml version")
      end
      local s = TryW()
      if s and Try"e" then ExpectS"ncoding="
        if not ParseAttributeValue():match"^[A-Za-z][A-Za-z0-9%._%-]*$" then
          Error("correct xml encoding")
        end
        s = TryW()
      end
      if s and Try"s" then ExpectS"tandalone="
        local v = ParseAttributeValue()
        if v ~= "yes" and v ~= "no" then
          Error("correct xml standalone")
        end
        ParseW()
      end
      Expect"?" Expect">"
      ParseW()
      Expect"<"
    end
    ExpectS"stream:stream"
    local attrs = ParseAttributes()
    attrs[0] = "stream:stream"
    coroutine.yield(attrs)
    Expect">"
    while true do
      ParseW()
      Expect"<"
      if Try"/" then
        ExpectS"stream:stream"
        ParseW()
        Expect">"
        print"Stream ended"
        return
      end
      local stanza = ParseElement()
      coroutine.yield(stanza)
    end
  end)
  r()
  return r
end

return NewParser
