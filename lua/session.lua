local xml = require"xml"
local NewDatabase = require"db"
local tcp = require"transport_tcp"
local xmppstream = require"xmppstream"
local util = require"util"
local Q = util.Q

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
  xml.Encode(st, buf, dir.."| ")
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
  local skipdrain
  local barejid = opts.localpart.."@"..opts.domainpart
  local idhooks = {}
  local nsfilters = {}
  local nonsfilter = {}
  local transport = tcp(opts.domainpart)

  local function AddXep(name)
    local t = require(name)(session)
    assert(type(t) == "table")
    xeplist[#xeplist+1] = t
    session[name] = t
    if t.nsfilter then
      local fils = t.nsfilter
      if type(fils) ~= "table" then fils = {fils} end
      for _, fil in ipairs(fils) do
        -- can only have one xep per xmlns
        assert(not nsfilters[fil])
        nsfilters[fil] = t
      end
      -- TODO: integrate nsfilters into calling hooks
    else
      nonsfilter[#nonsfilter+1] = t
    end
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
    if not transport.IsEmpty() then
      CallHooks("OnDrain")
    end
    if not transport.IsEmpty() then
      print"drain"
      transport.Flush()
    end
  end

  local function SendStanza(stanza)
    io.write("\x1b[32m")
    LogStanza(stanza, ">")
    print("\x1b[0m")
    transport.SendStanza(stanza)
  end

  -- TODO: only allow iq, message and presence don't return anything,
  -- except errors. Messages should be stored in database along with id
  -- so it can later be associated with the error
  local function HookId(id, fn)
    if not fn then fn, id = id, GenerateId() end
    idhooks[id] = idhooks[id] or {}
    table.insert(idhooks[id], fn)
    return id
  end

  local HandleXmpp = coroutine.wrap(function()
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

      local function HasFeature(tag, ns) return Q(features, tag, ns) end

      if opts.usetls and not hastls then
        assert(HasFeature("starttls", "urn:ietf:params:xml:ns:xmpp-tls"))
        SendStanza {[0]="starttls",
          xmlns="urn:ietf:params:xml:ns:xmpp-tls",
        }
        ExpectStanza("proceed", "urn:ietf:params:xml:ns:xmpp-tls")
        Handshake()
        stream = xmppstream()
        transport.OpenStream()
        hastls = true
        return HandleHeader()
      end

      local mechs = HasFeature("mechanisms", "urn:ietf:params:xml:ns:xmpp-sasl")
      if opts.saslmech and not hassasl then
        assert(mechs)
        assert(opts.password)
        local hasmech
        for _, mech in ipairs(mechs) do
          if type(mech) == "table" then
            assert(mech[0] == "mechanism" and #mech == 1)
            if mech[1] == opts.saslmech then hasmech = true break end
          end
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
        transport.OpenStream()
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
        local jidel = assert(Q(Q(bindres, "bind", "urn:ietf:params:xml:ns:xmpp-bind"), "jid"), "session: no bind jid")
        assert(bindres.id == id, "session: bind id not expected")
        fulljid = util.S(jidel[1])
        -- TODO: check if fulljid == opts.*part
      end
      isready = true
      CallHooks("OnFeatures", features)
    end

    transport.OpenStream()
    HandleHeader()
    session.SendStanza {[0]="presence",
      id=GenerateId(),
      {[0]="show", "away" },
    }

    while true do
      local st = GetStanza()
      CallHooks("OnGotStanza", st)
      if st.id and idhooks[st.id] then
        for _, hook in ipairs(idhooks[st.id]) do
          hook(st)
        end
        idhooks[st.id] = nil
      end
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
    HookId = HookId,
    GetFullJid = function() assert(isready) return fulljid end,
    GetBareJid = function() return barejid end,
  }
  AddXep("xep_disco")
  AddXep("xep_omemo")
  AddXep("xep_ping")
  AddXep("xep_receipts")
  AddXep("xep_sm")
  for _, xep in ipairs(xeplist) do
    local deps = xep.deps
    if type(deps) ~= "table" then deps = {deps} end
    for _, dep in ipairs(deps) do
      assert(session[dep], "session: dependency '"..dep
        .."' not met for '"..xep.."'")
    end
  end
  HandleXmpp()
  return session
end

return NewSession
