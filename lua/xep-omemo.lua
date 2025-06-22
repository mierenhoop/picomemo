local lomemo = require"lomemo"

local xmlns = "eu.siacs.conversations.axolotl"

return function()
  local self = {}
  -- After feature negotiation
  function self:Init()
    local id = HookPendingId(function() end)
    Write([[<iq xmlns="jabber:client" to="%s" type="get" id="%s">]],
    "admin@localhost", id)
    Write([[<pubsub xmlns="http://jabber.org/protocol/pubsub">]])
    Write([[<items node="eu.siacs.conversations.axolotl.devicelist" max_items="1"/>]])
    Write([[</pubsub></iq>]])
  end

  local function AnnounceBundle(bundle)
    local pks = {}
    for i = 1, #bundle.prekeys do
      pks[i] = X"preKeyPublic" {
        preKeyId = bundle.prekeys[i].id,
        bundle.prekeys[i].pk,
      }
    end
    local function Field(var, value, typ)
      Write([[<field var="%s"]])
      if typ then Write([[ type="%s"]], typ)
      Write([[><value>%s</value></field>]], value)
    end
    local id = HookPendingId(function() end)
    Write([[<iq type="set" id="%s"><pubsub>]], id)
    Write([[<publish node="%s.bundles:%d">]], xmlns, deviceid)
    Write([[<item id="current"><bundle xmlns="%s"]], xmlns)
    Write([[<signedPreKeyPublic signedPreKeyId="%d">]], bundle.spk_id)
    Write([[%b</signedPreKeyPublic>]], bundle.spk)
    Write([[<signedPreKeySignature>%b</signedPreKeySignature>]], bundle.spks)
    Write([[<identityKey>%b</identityKey>]], bundle.ik)
    -- TODO
    Write([[</bundle></item>]])
    Write([[</pubsub></iq]])
    local stanza = Xiq{
      type = "set",
      id = "randomid",
      Xpubsub {
        X"publish"{
          node = xmlns..".bundles:" .. deviceid,
          X"item" {
            id = "current",
            X"bundle" {
              xmlns = xmlns,
              X"signedPreKeyPublic" {
                signedPreKeyId = bundle.spk_id,
                EncodeBase64(bundle.spk),
              },
              X"signedPreKeySignature" { EncodeBase64(bundle.spks) },
              X"identityKey" { EncodeBase64(bundle.ik) },
              X"prekeys"(pks),
            }
          }
        },
        X"publish-options" {
          X"x" {
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
  return self
end
