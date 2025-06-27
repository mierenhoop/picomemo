#!/usr/bin/env lua5.4
require"native"
require"write"

local xmppstream = require"xmppstream"
local xep_sm = require"xep-sm"

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

local function NewSession()
  local session
  local stream
  local hastls, hassasl = false, false
  local isready
  local resumeid
  local streamattrs
  local fulljid
  local pending = {}
  local xep = {}
  local xeplist = {}
  local sendbuf = {}

  local function AddXep(name, fn)
    local t = fn(session)
    assert(type(t) == "table")
    xeplist[#xeplist+1] = t
    xep[name] = t
  end

  local Drain

  local inhook = {}
  local function CallHooks(fname, ...)
    -- prevent nested in same hook
    if inhook[fname] then return end
    inhook[fname] = true
    for _, x in ipairs(xeplist) do
      -- TODO: pcall
      -- TODO: iterate only over those that implement it
      if x[fname] then
        x[fname](...)
      end
    end
    Drain()
    inhook[fname] = false
  end

  Drain = function()
    if #sendbuf > 0 then
      CallHooks("OnDrain")
    end
    if #sendbuf > 0 then
      Send(table.concat(sendbuf))
      sendbuf = {}
    end
  end

  local function SendStanza(stanza)
    EncodeXml(stanza, sendbuf)
    Drain()
  end

  HandleXmpp = coroutine.wrap(function()
    local function GetStanza()
      local stanza = coroutine.yield()
      print(require"inspect"(stanza))
      -- TODO: only if successful
      return stanza
    end

    local function ExpectStanza(tag, ns)
      local stanza = GetStanza()
      -- TODO: right error
      assert(stanza[0] == tag and stanza.xmlns == ns)
      return stanza
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
      CallHooks("OnFeatures", features)
    end

    HandleHeader()
    isready = true

    while true do
      local st = GetStanza()
      CallHooks("OnGotStanza", st)
    end

    print(require"inspect"(features))
  end)
  session = {
    xep=xep,
    IsReady = function() return isready end,
    FeedStream = function(data)
      if not stream then stream = xmppstream() end
      local s = stream(data)
      while s do
        HandleXmpp(s)
        s = stream()
      end
    end,
    SendStanza = function(st)
      print(require"inspect"(st))
      EncodeXml(st, sendbuf)
      CallHooks("OnSendStanza", st)
    end,
    Drain = Drain,
  }
  AddXep("sm", xep_sm)
  HandleXmpp()
  return session
end

local session = NewSession()

function OnStdin()
  local msg = io.read("*l")
  if msg == "" then return end
  if session.IsReady() then
    session.SendStanza {[0]="message",
      id=GenerateId(),
      to="user@localhost",
      type="chat",
      ["xml:lang"]="en",
      {[0]="body", msg },
    }
    session.Drain()
  end
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
  session.FeedStream(data)
end

Connect("localhost", "localhost", "5222")
SendStreamHeader()
EventLoop()
--
