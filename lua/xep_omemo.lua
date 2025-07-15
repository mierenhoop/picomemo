local lomemo = require"lomemo"
local util = require"util"
local Q,S = util.Q,util.S

local xmlns = "eu.siacs.conversations.axolotl"

local ErrValid = "xep_omemo: XML not valid"
local ErrNoKey = "xep_omemo: no key for our device"

local function Field(var, value, typ)
  return {[0]="field",
    var = var,
    type=typ,
    {[0]="value", value},
  }
end
local publish_open = {[0]="publish-options",
  {[0]="x",
    xmlns = "jabber:x:data",
    type = "submit",
    Field("FORM_TYPE", "http://jabber.org/protocol/pubsub#publish-options", "hidden"),
    Field("pubsub#persist_items", "true"),
    Field("pubsub#access_model", "open"),
  },
}

local function V(...) assert(..., ErrValid) return ... end

return function(session)
  --local store
  --local function Initialize()
  --  session.db.exec[[
  --  create table if not exists omemo_store(data);
  --  create table if not exists omemo_session(data);
  --  ]]
  --  local data = db.urow"select data from omemo_store"
  --  if data then
  --    store = lomemo.DeserializeStore(s)
  --  else
  --    store = lomemo.SetupStore()
  --  end
  --end
  local store = lomemo.SetupStore()
  ---@type {[string]: {[integer]: lomemo.Session}}
  local sessions = {}

  -- TODO: put in xep_pep?
  local function MakePublishStanza(id, node, item)
    return {[0]="iq",
      --from=session.GetFullJid(),
      id=id,
      type="set",
      {[0]="pubsub",
        xmlns="http://jabber.org/protocol/pubsub",
        {[0]="publish",
          node=node,
          item,
        },
        publish_open,
      },
    }
  end

  local deviceid = 7

  local function AnnounceBundle()
    local bundle = store:GetBundle()
    local pks = {[0]="prekeys"}
    for i = 1, #bundle.prekeys do
      pks[i] = {[0]="preKeyPublic",
        preKeyId = bundle.prekeys[i].id,
        EncodeBase64(bundle.prekeys[i].pk),
      }
    end
    local id = session.GenerateId()
    local stanza = MakePublishStanza(id, xmlns..".bundles:" .. tostring(deviceid),
    {[0]="item",
      id = "current",
      {[0]="bundle",
        xmlns = xmlns,
        {[0]="signedPreKeyPublic",
          signedPreKeyId = bundle.spk_id,
          EncodeBase64(bundle.spk),
        },
        {[0]="signedPreKeySignature", EncodeBase64(bundle.spks) },
        {[0]="identityKey", EncodeBase64(bundle.ik) },
        pks,
      },
    })
    session.SendStanza(stanza)
  end

  local function GetBundle(to, rid, hook)
    session.SendStanza {[0]="iq",
      to=to, type="get", id=session.HookId(hook),
      {[0]="pubsub",xmlns="http://jabber.org/protocol/pubsub",
        {[0]="items",node=xmlns..".bundles:"..rid, max_items="1"}
      },
    }
  end

  local function ParseBundle(st, to, rid)
    local bundle = Q(Q(Q(Q(st, "pubsub", "http://jabber.org/protocol/pubsub"), "items"), "item", "http://jabber.org/protocol/pubsub"), "bundle", xmlns)
    assert(bundle, "xep_omemo: no bundle")
    local spk = Q(bundle, "signedPreKeyPublic")
    local spk_id = V(tonumber(spk.signedPreKeyId))
    spk = DecodeBase64(S(spk[1]))
    local spks = assert(DecodeBase64(S(V(Q(bundle, "signedPreKeySignature"))[1])))
    local ik = assert(DecodeBase64(S(V(Q(bundle, "identityKey"))[1])))
    local prekeys = V(Q(bundle, "prekeys"))
    -- filter out all whitespace
    local j = 0
    for i = 1, #prekeys do
      if type(prekeys[i]) == "table" then j=j+1 prekeys[j]=prekeys[i] end
    end
    assert(j>0, "xep_omemo: no prekeys")
    local pk = prekeys[math.random(j)]
    local pk_id = V(tonumber(pk.preKeyId))
    pk = assert(DecodeBase64(S(pk[1])))
    return {
      spks = spks,
      spk = spk,
      ik = ik,
      pk = pk,
      spk_id = spk_id,
      pk_id = pk_id,
    }
  end

  local function DecryptMessage(st)
    local enc = V(Q(st, "encrypted", xmlns))
    local header = V(Q(enc, "header"))
    local payload = Q(enc, "payload")
    if payload then payload = payload[1] end
    local foundkey
    for _, key in ipairs(header) do
      if type(key) == "table" and tonumber(key.rid) == deviceid then
        foundkey = key
        break
      end
    end
    assert(foundkey, ErrNoKey)
    -- TODO: something more reliable
    local from = S(st.from):match("^([^/]*)")
    local ses = sessions[from]
    if not ses then
      ses = {}
      sessions[from] = ses
    end
    local sid = V(tonumber(header.sid))
    if not ses[sid] then
      ses[sid] = lomemo.NewSession()
    end
    ses = ses[sid]
    local enckey = assert(DecodeBase64(S(foundkey[1])))
    local ispk = foundkey.prekey == "true"
    -- TODO: cbs
    local key = assert(ses:DecryptKey(store, ispk, enckey, {load=function() end, store=function() end}))
    if payload then
      local encmsg = assert(DecodeBase64(payload))
      local iv = assert(DecodeBase64(S(V(Q(header,"iv"))[1])))
      local msg = assert(lomemo.DecryptMessage(encmsg, key, iv))
      return msg
    end
  end
  local function GetDeviceList(to, hook)
    -- TODO: cache and use cached if exists
    session.SendStanza {[0]="iq",
      to=to, type="get", id=session.HookId(hook),
      {[0]="pubsub",xmlns="http://jabber.org/protocol/pubsub",
        {[0]="items",node=xmlns..".devicelist", max_items="1"}
      },
    }
  end
  local function QueryList(st)
    return Q(Q(Q(Q(st, "pubsub", "http://jabber.org/protocol/pubsub"), "items"), "item", "http://jabber.org/protocol/pubsub"), "list", xmlns)
  end
  -- returns list of new devices
  local function HandleRemoteDeviceList(st, to)
    local list = QueryList(st)
    local ss = sessions[to]
    if not ss then
      ss = {}
      sessions[to] = ss
    end
    local new = {}
    if list then
      for _, dev in ipairs(list) do
        local id = tonumber(dev.id)
        if ss[id] == nil then
          new[#new+1] = id
        end
      end
    end
    return new
  end
  local function HandleOurDeviceList(st)
      local list = QueryList(st)
      local found
      if list then
        for _, dev in ipairs(list) do
          if tonumber(dev.id) == deviceid then found = true end
        end
      else
        list = {[0]="list",xmlns=xmlns}
      end
      if not found then
        -- TODO: should we verify the format of the existing device list entries?
        list[#list+1] = {[0]="device", id=tostring(deviceid)}
        local id = session.HookId(function(st2)
          -- TODO: check if success
        end)
        local st = MakePublishStanza(id, xmlns..".devicelist", {
          [0]="item",
          id="current",
          list,
        })
        session.SendStanza(st)
      end
      -- TODO: don't always announce bundle
      AnnounceBundle()
  end

  local function SendMessage(msg, to)
      assert(store)
      -- TODO: first get all updated bundles (for new device ids)
      local ss = sessions[to]
      assert(ss)
      local encmsg, key, iv = assert(lomemo.EncryptMessage(msg))
      local header = {[0]="header",
        sid=tostring(deviceid),
        {[0]="iv",EncodeBase64(iv)},
      }
      -- TODO: check if at least one
      for rid, ses in pairs(ss) do
        local enckey, ispk = ses:EncryptKey(store, key)
        -- TODO: persist newly updated session
        header[#header+1] = {[0]="key",
          prekey = ispk and "true" or nil,
          rid = tostring(rid),
          EncodeBase64(enckey),
        }
      end
      session.SendStanza {[0]="message",
        to=to,
        type="chat",
        {[0]="encrypted",
          xmlns=xmlns,
          header,
          {[0]="payload", EncodeBase64(encmsg)},
        },
        {[0]="encryption",xmlns="urn:xmpp:eme:0",name="OMEMO",namespace=xmlns},
        {[0]="body","You received a message encrypted with OMEMO but your client doesn't support OMEMO."},
        -- TODO: these should be added outside this module
        {[0]="request",xmlns="urn:xmpp:receipts"},
        {[0]="markable",xmlns="urn:xmpp:chat-markers:0"},
        {[0]="store",xmlns="urn:xmpp:hints"},
      }
  end
  return {
    --deps= "xep_pep",
    nsfilter = xmlns,
    OnFeatures = function(features)
      GetDeviceList(session.GetBareJid(), HandleOurDeviceList)
    end,
    OnGotStanza = function(st)
      -- TODO: do something better
      if st[0] == "message" and Q(st, "encrypted", xmlns) then
        print("Got omemo: ", DecryptMessage(st))
      end
    end,
    -- TODO: we can make a coroutine instead of the callbacks
    SendMessage = function(msg, to)
      GetDeviceList(to, function(st)
        local new = HandleRemoteDeviceList(st, to)
        local ss = sessions[to]
        assert(ss)
        if #new == 0 then
          SendMessage(msg, to)
        end
        -- TODO: have timeout for when never done
        local bundles = {}
        for _, id in ipairs(new) do
          bundles[id] = false
          GetBundle(to, id, function(st)
            bundles[id] = ParseBundle(st)
            local notdone
            for _, b in pairs(bundles) do
              if not b then notdone = true end
            end
            if not notdone then
              for id, b in pairs(bundles) do
                -- in the meantime, a session could already have been created
                if not ss[id] then
                  ss[id] = lomemo.InitFromBundle(store, b)
                end
              end
              SendMessage(msg, to)
            end
          end)
        end
      end)
    end,
  }
end
