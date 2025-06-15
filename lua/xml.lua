#!/usr/bin/env lua5.4
require"native"

local input = [[
<?xml version='1.0'?>
<stream:stream
  from='im.example.com'
  id='++TR84Sm6A3hnt3Q065SnAbbk3Y='
  to='juliet@im.example.com'
  version='1.0'
  xml:lang='en'
  xmlns='jabber:client'
  xmlns:stream='http://etherx.jabber.org/streams'>
<stream:features>
<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'>
	<required/>
  </starttls>
</stream:features>
]]
local pos = 1

local tagstack = {}

local function Find(patt, err)
  local b, e = input:find(patt, pos)
  if b then
    pos = e+1
    return input:sub(b,e)
  else
    if err then
      error("expected "..err.." at "..pos)
    end
  end
end

local function W()
  -- Space also includes ascii 11 and 12 while XML spec doesn't recognize
  -- those
  local b = input:find("%S", pos)
  if not b then coroutine.yield() end
  pos = b
end

local function Get()
  if pos <= #input then
    local c= input:sub(pos,pos)
    pos=pos+1
    return c
  else
    coroutine.yield()
  end
end

function TryEnd(name)
end

function ParseName()
    return Find("^[:%a][:%w%-%.]*", "name")
end

function ReplaceEntities(s)
  return (s:gsub("&([^;]*;?)", function(ent)
    local rep = ({["amp;"]="&",["lt;"]="<",["gt;"]=">",["apos;"]="'",["quot;"]="\""})[ent]
    if not rep then error("entity") end
    return rep
  end))
end

function ParseElement()
  Find("^<", "opening tag")
  local el = {}
  local name = ParseName()
  local attrs = {}
  el[0] = name
  el.attrs = attrs
  while true do
    local e = Find("^%s*/?>")
    if e then
      if e:sub(-2,-2) ~= "/" then
        table.insert(tagstack, el)
      end
      break
    end
    Find("^%s+", "space")
    local key = ParseName()
    -- We don't handle UTF-8
    Find("^=", "equals")
    local quote = Find("^['\"]", "quote")
    local content = ReplaceEntities(Find("^[^"..quote.."]*"))
    Find("^"..quote, "ending quote")
    attrs[key] = content
  end
  return el
end

local co = coroutine.wrap(function()
  W()
  -- TODO: maybe check contents, but in correct document this will always be
  -- fine (as no ? is allowed inside the PI)
  Find("^<%?xml.-%?>")
  W()
  local streamstart = ParseElement()
  assert(streamstart[0] == "stream:stream" and streamstart.attrs.xmlns == "jabber:client")
  while #tagstack > 0 do
    print(#tagstack)
    W()
    -- Content only allowed within stanzas
    if #tagstack > 1 then
      -- Strip spaces around the content
      local content = Find("^[^<]*[^%s<]")
      if content then
        table.insert(tagstack[#tagstack-1], ReplaceEntities(content))
      end
      W()
    end
    if Find("^</") then
      local name = ParseName()
      local el = table.remove(tagstack)
      assert(el[0] == name)
      Find("^>", "closing tag")
      if #tagstack == 1 then
        coroutine.yield(el)
      end
    else
      local el = ParseElement()
      if #tagstack > 2 then
        table.insert(tagstack[#tagstack-1], el)
      end
    end
  end
end)

print(require"inspect"(co()))
print(require"inspect"(co()))
print(require"inspect"(tagstack))

function OnStdin()
  print(io.read"*l")
  --local store = omemo.SetupStore()

  --local spks = stanza.spks -- etc.
  --local session, err = omemo.InitFromBundle(store, {
  --  spks, spk, ik, pk, pk_id, spk_id,
  --})
  --local deckey, err = session:DecryptKey(store, isprekey, enckey)
  --local decmsg, err = omemo.DecryptMessage(deckey, iv, encmsg)

  --local serversig, clientsig, clientproof = CalculateScramSha({}, pwd, salt, itrs)
  --EncodeBase64()
  --DecodeBase64()
  --GetRandomBytes(20)
end

function OnReceive(data)
end
--EventLoop()
