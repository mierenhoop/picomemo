#!/usr/bin/env lua5.4
require"native"
require"write"

local xmppstream = require"xmppstream"
local stream

--local xepfns = {
--  ["eu.siacs.conversations.axolotl"] = require"xep-omemo",
--}

local function SendStreamHeader()
  Write([[<?xml version="1.0"?><stream:stream]])
  Write([[ version="1.0" xml:lang="en" xmlns="jabber:client"]])
  Write([[ from="%s"]], "admin@localhost")
  Write([[ to="%s"]], "localhost")
  Write([[ xmlns:stream="http://etherx.jabber.org/streams">]])
  Flush()
end

local function GenerateId()
  -- TODO: maybe use something else
  local t = {}
  for i = 1, 20 do
    t[i] = string.format("%02x", math.random(0,255))
  end
  return table.concat(t)
end

local HandleXmpp = coroutine.wrap(function()
  local GetStanza = coroutine.yield

  local function ExpectStanza(tag, ns)
    local stanza = GetStanza()
    -- TODO: right error
    assert(stanza[0] == tag and stanza.xmlns == ns)
    return stanza
  end

  local hastls, hassasl = false, false
  local resumeid
  local streamattrs
  local smenabled = false
  local fulljid

  local pending = {}
  --local xeps = {}

  --for xmlns, xepfn in pairs(xepfns) do
  --  xeps[xmlns] = xepfn()
  --end
  local function SendStanza(stanza)
    local buf = {}
    EncodeXml(stanza, buf)
    Send(table.concat(buf))
    Flush()
    -- TODO: stanzacount += 1 for stream acknowledgement
  end


  local function HandleHeader()
    streamattrs = ExpectStanza("stream:stream", "jabber:client")
    local features = ExpectStanza("stream:features", nil)

    local function HasFeature(tag, ns)
      for _, feature in ipairs(features) do
        -- TODO: better error
        assert(type(feature) == "table")
        if feature[0] == tag and feature.xmlns == ns then
          return feature
        end
      end
    end

    if not hastls and HasFeature("starttls", "urn:ietf:params:xml:ns:xmpp-tls") then
      SendStanza {[0]="starttls",
        xmlns="urn:ietf:params:xml:ns:xmpp-tls",
      }
      ExpectStanza("proceed", "urn:ietf:params:xml:ns:xmpp-tls")
      Handshake()
      stream = xmppstream()
      SendStreamHeader()
      hastls = true
      return HandleHeader()
    end

    local mechs = HasFeature("mechanisms", "urn:ietf:params:xml:ns:xmpp-sasl")
    if not hassasl and mechs then
      for _, mech in ipairs(mechs) do
        assert(mech[0] == "mechanism" and #mech == 1)
        -- TODO: select best fitting mech
      end
      SendStanza{[0]="auth",
        xmlns="urn:ietf:params:xml:ns:xmpp-sasl",
        mechanism="PLAIN",
        EncodeBase64("\0admin\0adminpass"),
      }
      ExpectStanza("success", "urn:ietf:params:xml:ns:xmpp-sasl")
      stream = xmppstream()
      SendStreamHeader()
      hassasl = true
      return HandleHeader()
    end

    if HasFeature("bind", "urn:ietf:params:xml:ns:xmpp-bind") then
      local id = GenerateId()
      SendStanza {[0]="iq", id=id, type="set",
        {[0]="bind",xmlns="urn:ietf:params:xml:ns:xmpp-bind"}
      }
      local bindres = ExpectStanza("iq", nil)
      assert(#bindres == 1 and bindres[1][0] == "bind" and bindres[1].xmlns == "urn:ietf:params:xml:ns:xmpp-bind")
      assert(bindres.id == id)
      fulljid = bindres[1][1]
    end

    if HasFeature("sm", "urn:xmpp:sm:3") then
      SendStanza {[0]="enable",xmlns="urn:xmpp:sm:3",resume="true" }
      --local enabled = ExpectStanza("enabled", "urn:xmpp:sm:3")
      --resumeid = assert(enabled.id)
    end
  end

  HandleHeader()

  Send[[<presence/>]]

  --for xmlns, xep in pairs(xeps) do
  --  if xep.Init then
  --    xep:Init()
  --  end
  --end

  while true do
    local s = GetStanza()
    if s.xmlns == "urn:xmpp:sm:3" then
      if s[0] == "enabled" then assert(not smenabled) smenabled = true end
    end
  end

  --print(require"inspect"(features))
end)
HandleXmpp()

function OnStdin()
  local msg = io.read("*l")
  --Send([[<message></message>]])
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
  print(data)
  if not stream then stream = xmppstream() end
  local s = stream(data)
  while s do
    HandleXmpp(s)
    s = stream()
  end
end

Connect("localhost", "localhost", "5222")
SendStreamHeader()
EventLoop()
--
