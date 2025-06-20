#!/usr/bin/env lua5.4
require"native"

local xmppstream = require"xmppstream"
local stream

local function SendStreamHeader()
  Send([[
<?xml version='1.0'?>
<stream:stream
  from='admin@localhost'
  to='localhost'
  version='1.0'
  xml:lang='en'
  xmlns='jabber:client'
  xmlns:stream='http://etherx.jabber.org/streams'>]])
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
      Send[[<starttls xmlns="urn:ietf:params:xml:ns:xmpp-tls"/>]]
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

      Send(([[<auth xmlns="urn:ietf:params:xml:ns:xmpp-sasl" mechanism="PLAIN">%s</auth>]]):format(EncodeBase64("\0admin\0adminpass")))
      ExpectStanza("success", "urn:ietf:params:xml:ns:xmpp-sasl")
      stream = xmppstream()
      SendStreamHeader()
      hassasl = true
      return HandleHeader()
    end

    if HasFeature("bind", "urn:ietf:params:xml:ns:xmpp-bind") then
      -- TODO: is it possible that we receive data from the server before
      -- handling bind and smacks?
      -- TODO: generate iq id
      Send[[<iq id="tn281v37" type="set"><bind xmlns="urn:ietf:params:xml:ns:xmpp-bind"/></iq>]]
      local bindres = ExpectStanza("iq", nil)
      assert(#bindres == 1 and bindres[1][0] == "bind" and bindres[1].xmlns == "urn:ietf:params:xml:ns:xmpp-bind")
    end

    if HasFeature("sm", "urn:xmpp:sm:3") then
      Send[[<enable xmlns="urn:xmpp:sm:3" resume="true"/>]]
      local enabled = ExpectStanza("enabled", "urn:xmpp:sm:3")
      resumeid = assert(enabled.id)
    end
  end

  HandleHeader()

  while true do
    GetStanza()
  end

  --print(require"inspect"(features))
end)
HandleXmpp()

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
