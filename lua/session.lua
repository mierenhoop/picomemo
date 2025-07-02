require"write"
local NewDatabase = require"db"

local xmppstream = require"xmppstream"

local function GenerateId()
  -- TODO: maybe use something else
  local t = {}
  for i = 1, 20 do
    t[i] = string.format("%02x", math.random(0,255))
  end
  return table.concat(t)
end

local function LogStanza(st, dir)
  local buf = {}
  EncodeXml(st, buf, dir.."| ")
  io.write(table.concat(buf))
end

local function NewSession(opts)
  local session
  local stream
  local hastls, hassasl
  local isready
  local resumeid
  local streamattrs
  local fulljid
  local pending = {}
  local xeplist = {}
  local sendbuf = {}
  local skipdrain
  local barejid = opts.localpart.."@"..opts.domainpart

  local function AddXep(name)
    local t = require(name)(session)
    assert(type(t) == "table")
    xeplist[#xeplist+1] = t
    session[name] = t
  end

  local inhook = {}
  local function CallHooks(fname, ...)
    local save = skipdrain
    -- prevent nested in same hook
    if inhook[fname] then return end
    inhook[fname] = true
    for _, x in ipairs(xeplist) do
      -- TODO: pcall
      -- TODO: iterate only over those that implement it
      if x[fname] then
        skipdrain = true
        x[fname](...)
        if not save then skipdrain = false end
      end
    end
    inhook[fname] = false
  end

  local function Drain()
    if #sendbuf > 0 then
      CallHooks("OnDrain")
    end
    if #sendbuf > 0 then
      print"drain"
      Send(table.concat(sendbuf))
      sendbuf = {}
    end
  end

  local function SendStanza(stanza)
    io.write("\x1b[32m")
    LogStanza(stanza, ">")
    print("\x1b[0m")
    EncodeXml(stanza, sendbuf)
  end

  local function SendStreamHeader()
    table.insert(sendbuf, "<?xml version=\"1.0\"?>")
    EncodeXml({[0]="stream:stream",
      version = "1.0",
      ["xml:lang"] = "en",
      xmlns = "jabber:client",
      -- TODO: from?
      to = opts.domainpart,
      ["xmlns:stream"] = "http://etherx.jabber.org/streams",
    }, sendbuf)
    -- HACK
    assert(sendbuf[#sendbuf] == "/>")
    sendbuf[#sendbuf] = ">"
  end

  HandleXmpp = coroutine.wrap(function()
    local function GetStanza()
      Drain()
      local stanza = coroutine.yield()
      io.write("\x1b[34m")
      LogStanza(stanza, "<")
      print("\x1b[0m")
      -- TODO: only if successful
      return stanza
    end

    local function ExpectStanza(tag, ns)
      local stanza = GetStanza()
      -- TODO: right error
      assert(stanza[0] == tag and stanza.xmlns == ns)
      return stanza
    end

    local function Close()
      -- TODO: only send once, while not calling the hooks as we don't
      -- want <r/> after stream closing, maybe have isclosed variable
      Drain()
      Send("</stream:stream>")
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

      if opts.usetls and not hastls then
        assert(HasFeature("starttls", "urn:ietf:params:xml:ns:xmpp-tls"))
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
      if opts.saslmech and not hassasl then
        assert(mechs)
        assert(opts.password)
        local hasmech
        for _, mech in ipairs(mechs) do
          assert(mech[0] == "mechanism" and #mech == 1)
          -- TODO: what if spaces around XML content?
          if mech[1] == opts.saslmech then hasmech = true break end
        end
        assert(hasmech)
        if opts.saslmech == "PLAIN" then
          -- TODO: saslprep
          local auth = "\0"..opts.localpart.."\0"..opts.password
          SendStanza{[0]="auth",
            xmlns="urn:ietf:params:xml:ns:xmpp-sasl",
            mechanism="PLAIN",
            EncodeBase64(auth),
          }
          ExpectStanza("success", "urn:ietf:params:xml:ns:xmpp-sasl")
        else
          -- TODO: SCRAM-SHA
          assert(false)
        end
        stream = xmppstream()
        SendStreamHeader()
        hassasl = true
        return HandleHeader()
      end

      if HasFeature("bind", "urn:ietf:params:xml:ns:xmpp-bind") then
        local id = GenerateId()
        SendStanza {[0]="iq", id=id, type="set",
          {[0]="bind",xmlns="urn:ietf:params:xml:ns:xmpp-bind",
            {[0]="resource", opts.resourcepart},
          }
        }
        local bindres = ExpectStanza("iq", nil)
        assert(#bindres == 1 and bindres[1][0] == "bind" and bindres[1].xmlns == "urn:ietf:params:xml:ns:xmpp-bind")
        assert(bindres.id == id)
        fulljid = bindres[1][1][1]
        -- TODO: check if fulljid == opts.*part
      end
      isready = true
      CallHooks("OnFeatures", features)
    end

    SendStreamHeader()
    HandleHeader()
    SendStanza {[0]="presence",
      id=GenerateId(),
      {[0]="show", "away" },
    }

    while true do
      local st = GetStanza()
      CallHooks("OnGotStanza", st)
    end
  end)
  session = {
    opts = opts,
    db = NewDatabase(":memory:"),
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
      assert(isready)
      SendStanza(st)
      local save = skipdrain
      CallHooks("OnSendStanza", st)
      -- Only Drain when this SendStanza call is not called by a hook
      if not save then Drain() end
    end,
    Drain = Drain,
    GenerateId = GenerateId,
    GetFullJid = function() assert(isready) return fulljid end,
    GetBareJid = function() return barejid end,
  }
  AddXep("xep_sm")
  AddXep("xep_ping")
  AddXep("xep_omemo")
  HandleXmpp()
  return session
end

return NewSession
