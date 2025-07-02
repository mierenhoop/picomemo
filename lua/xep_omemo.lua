local lomemo = require"lomemo"

local xmlns = "eu.siacs.conversations.axolotl"

-- TODO: put this in a more top level
local function SafeIndex(st, i, ...)
  if not i then return st end
  for j = 1, #st do
    if st[j][0] == i then
      local ok = true
      for k, v in pairs(i) do
        if st[k] ~= v then
          ok = false
          break
        end
      end
      if ok then
        return SafeIndex(st[j], ...)
      end
    end
  end
end

local function Q(st, name, xmlns)
  if st then
    for _, c in ipairs(st) do
      if c[0] == name and c.xmlns == xmlns then
        return c
      end
    end
  end
end

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
  local dlreqid
  local store
  ---@type {[string]: {[integer]: lomemo.Session}}
  local sessions = {}

  local function PublishStanza(node, item)
    local id = session.GenerateId()
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
      },
    }, id
  end

  local deviceid = 7

  local function AnnounceBundle(bundle)
    local pks = {[0]="prekeys"}
    for i = 1, #bundle.prekeys do
      pks[i] = {[0]="preKeyPublic",
        preKeyId = bundle.prekeys[i].id,
        EncodeBase64(bundle.prekeys[i].pk),
      }
    end
    local function Field(var, value, typ)
      return {[0]="field",
        var = var,
        type=typ,
        {[0]="value", value},
      }
    end
    local id = session.GenerateId()
    local stanza = {[0]="iq",
      type = "set",
      id = id,
      {[0]="pubsub",
        xmlns="http://jabber.org/protocol/pubsub",
        {[0]="publish",
          node = xmlns..".bundles:" .. tostring(deviceid),
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
          },
        },
        {[0]="publish-options",
          {[0]="x",
            xmlns = "jabber:x:data",
            type = "submit",
            Field("FORM_TYPE", "http://jabber.org/protocol/pubsub#publish-options", "hidden"),
            Field("pubsub#persist_items", "true"),
            Field("pubsub#access_model", "open"),
          },
        },
      },
    }
    session.SendStanza(stanza)
  end
  local function DecryptMessage(st)
    local enc = Q(st, "encrypted", xmlns)
    local header = Q(enc, "header")
    local payload = Q(enc, "payload")[1]
    local foundkey
    for _, key in ipairs(header) do
      if key.rid == deviceid then
        foundkey = key
        break
      end
    end
    assert(foundkey)
    -- TODO: resource part included?
    local ses = sessions[st.from]
    if not ses then
      ses = {}
      sessions[st.from] = ses
    end
    local sid = tonumber(header.sid)
    if not ses[sid] then
      ses[sid] = lomemo.NewSession()
    end
    ses = ses[sid]
    local enckey = DecodeBase64(foundkey[1])
    -- TODO: prekey might be other than true
    local ispk = foundkey.prekey == "true"
    -- TODO: cbs
    local key = ses:DecryptKey(store, ispk, enckey, {})
    local encmsg = DecodeBase64(payload)
    local iv = DecodeBase64(header.iv)
    local msg = lomemo.DecryptMessage(encmsg, key, iv)
    return msg
  end
  local function GetDeviceList(to)
    dlreqid = session.GenerateId()
    session.SendStanza {[0]="iq",
      to=to, type="get", id=dlreqid,
      {[0]="pubsub",xmlns="http://jabber.org/protocol/pubsub",
        {[0]="items",node=xmlns..".devicelist", max_items="1"}
      },
    }
  end
  return {
    OnFeatures = function(features)
      GetDeviceList(session.GetBareJid())
      -- TODO: only publish after got our list
      --store = lomemo.SetupStore()
      --local bundle = store:GetBundle()
      --AnnounceBundle(bundle)
    end,
    OnGotStanza = function(st)
      if st[0] == "iq" and st.id and st.id == dlreqid then
        local list = Q(Q(Q(Q(st, "pubsub", "http://jabber.org/protocol/pubsub"), "items"), "item", "http://jabber.org/protocol/pubsub"), "list", xmlns)
        local found
        if list then
          for _, dev in ipairs(list) do
            if tonumber(dev.id) == deviceid then found = true end
          end
        else
          list = {[0]="list",xmlns=xmlns}
        end
        if not found then
          list[#list+1] = {[0]="device", id=tostring(deviceid)}
          local st, id = PublishStanza(xmlns..".devicelist", {
            [0]="item",
            id="current",
            list,
          })
          session.SendStanza(st)
        end
      end
      --[[
      if st[0] == "message" and st.id == dlreqid then
        local list = {}
        local devicelist = SafeIndex(st, {"event",xmlns="http://jabber.org/protocol/pubsub#event"},{"items", node=xmlns..".devicelist"}, {"item",id="current"},{"list", xmlns=xmlns})
        for i = 1, #devicelist do
          if type(devicelist[i]) == "table" then
            local id = tonumber(devicelist[i].id)
            if id and 0 < id and id < 2^32 then
              table.insert(list, id)
            end
          end
        end
      end
      ]]
    end,
    SendMessage = function(msg, to)
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
    end,
  }
end
