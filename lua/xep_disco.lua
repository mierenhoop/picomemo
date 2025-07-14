local util = require"util"
local xmlns_info  = "http://jabber.org/protocol/disco#info"
local xmlns_items = "http://jabber.org/protocol/disco#items"

return function(session)
  local features = {}
  return {
    xepfilter = {xmlns_info, xmlns_items},
    OnGotStanza = function(st)
      if st[0] == "iq" and util.Q("query", xmlns_info) then
      end
    end,
    Register = function(feature)
      table.insert(features, feature)
      table.sort(features)
    end,
  }
end
