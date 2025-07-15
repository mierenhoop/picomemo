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
    error("xmppstream: expected " .. err .. " at char " .. pos)
  end

  local function Expect(patt, err)
    local c = Try(patt)
    if not c then Error(err or patt) end
    return c
  end

  local function TryC(c)
    if Peek() == c then
      pos = pos + 1
      return c
    end
  end

  local function ExpectC(c, err)
    if not TryC(c) then Error(err or c) end
    return c
  end

  local function FindC(c)
    repeat
      WaitForData()
      local b = inputs[#inputs]:find(c, pos, true)
      pos = (b or #inputs[#inputs]) + 1
    until b
    return c
  end

  local function Find(patt)
    repeat
      WaitForData()
      local b = inputs[#inputs]:find(patt, pos, true)
      pos = (b or #inputs[#inputs]) + 1
    until b
    return inputs[#inputs]:sub(b,b)
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
    -- TODO: optimize
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
    local q = TryC'"' or ExpectC"'"
    Save()
    FindC(q)
    -- TODO: remove sub
    local s = GetSaved():sub(1,-2)
    s = ReplaceEntities(s)
    return s
  end

  -- Parse from <elem (excluding space) until expected / or >
  local function ParseAttributes()
    local attrs = {}
    while TryW() do
      local key = ParseName()
      if not key then break end
      ExpectC"="
      local val = ParseAttributeValue()
      attrs[key] = val
    end
    return attrs
  end

  local function ParseCdata()
      ExpectC"[" ExpectS"CDATA" ExpectC"["
      Save()
      while true do
        FindC"]"
        local i=1
        while TryC"]" do
          i=i+1
        end
        if i >= 2 and TryC">" then
          return GetSaved():sub(1, -4)
        end
      end
  end

  -- Parse after opening <
  -- TODO: not have recursion?
  -- TODO: extract non-stream element parser for BOSH, websocket, SCE
  local function ParseElement()
    local name = ParseName()
    if not name then Error("opening tag") end
    local attrs = ParseAttributes()
    attrs[0] = name
    if TryC"/" then
      ExpectC">"
      return attrs
    end
    ExpectC">"
    local content,istab
    while true do
      -- TODO: do we want to omit the whitespace before possible content?
      --ParseW()
      Save()
      -- TODO: xml chars
      if not TryC"<" then
        FindC"<"
        -- TODO: remove sub
        local cc = GetSaved():sub(1,-2)
        -- TODO: strip whitespace?
        cc = ReplaceEntities(cc--[[:gsub(Wpatt.."*$", "")]])
        if not content then content = cc
        elseif istab then content[#content+1] = cc
        else content,istab = {content,cc},true
        end
      else
        Unsave()
      end
      if TryC"!" then
        local cc = ParseCdata()
        if not content then content = cc
        elseif istab then content[#content+1] = cc
        else content,istab = {content,cc},true
        end
      else
        if content then
          attrs[#attrs+1] = istab and table.concat(content) or content
        end
        if TryC"/" then
          if ParseName() ~= name then Error("same closing tag as opening tag") end
          ParseW()
          ExpectC">"
          break
        end
        attrs[#attrs+1] = ParseElement()
      end
    end
    return attrs
  end

  local r = coroutine.wrap(function()
    local s = TryW()
    ExpectC"<"
    if not s and TryC"?" then
      ExpectS"xml"
      ExpectW()
      ExpectS"version="
      -- TODO: match"^[uU][tT][fF]%-8$" for XMPP
      if not ParseAttributeValue():match"^1%.%d+$" then
        Error("correct xml version")
      end
      local s = TryW()
      if s and TryC"e" then ExpectS"ncoding="
        if not ParseAttributeValue():match"^[A-Za-z][A-Za-z0-9%._%-]*$" then
          Error("correct xml encoding")
        end
        s = TryW()
      end
      if s and TryC"s" then ExpectS"tandalone="
        local v = ParseAttributeValue()
        if v ~= "yes" and v ~= "no" then
          Error("correct xml standalone")
        end
        ParseW()
      end
      ExpectC"?" ExpectC">"
      ParseW()
      ExpectC"<"
    end
    ExpectS"stream:stream"
    local attrs = ParseAttributes()
    attrs[0] = "stream:stream"
    coroutine.yield(attrs)
    ExpectC">"
    while true do
      ParseW()
      ExpectC"<"
      if TryC"/" then
        ExpectS"stream:stream"
        ParseW()
        ExpectC">"
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
