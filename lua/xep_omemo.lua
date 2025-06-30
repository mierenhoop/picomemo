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

  local function AnnounceBundle(bundle)
    local pks = {}
    for i = 1, #bundle.prekeys do
      pks[i] = {[0]="preKeyPublic",
        preKeyId = bundle.prekeys[i].id,
        bundle.prekeys[i].pk,
      }
    end
    local function Field(var, value, typ)
      return {[0]="field",
        var = var,
        type=typ,
        {[0]="field", value},
      }
    end
    local id = session.GenerateId()
    local stanza = {[0]="iq",
      type = "set",
      id = id,
      {[0]="pubsub",
        {[0]="publish",
          node = xmlns..".bundles:" .. deviceid,
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
              {[0]="prekeys", pks},
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
  return {
    OnFeatures = function(features)
      local id = session.GenerateId()
      session.SendStanza {[0]="iq",
        xmlns="jabber:client", to="admin@localhost", type="get", id=id,
        {[0]="pubsub",xmlns="http://jabber.org/protocol/pubsub",
          {[0]="items",node=xmlns..".devicelist", --[[max_items="1"]]}
        },
      }
      --AnnounceBundle()
    end,
    OnGotStanza = function(st)
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
    end,
  }
end
