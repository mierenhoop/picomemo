local lomemo = require"lomemo"

local xmlns = "eu.siacs.conversations.axolotl"

return function(session)
  local function AnnounceBundle(bundle)
    local pks = {}
    for i = 1, #bundle.prekeys do
      pks[i] = X"preKeyPublic" {
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
    local id = HookPendingId(function() end)
    local stanza = {[0]="iq",
      type = "set",
      id = "randomid",
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
  }
end
