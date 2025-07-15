local xml = require"xml"

local xmlns = "urn:ietf:params:xml:ns:xmpp-framing"

local function TransformIncomingStanza(st)
  if st[0]:sub(1,7) == "stream:" then
    --assert(st["xmlns:stream"] == "http://etherx.jabber.org/streams")
    st["xmlns:stream"] = nil
    return
  end

  if st[0] == "features" then st[0] = "stream:features"
  elseif st[0] == "error" then st[0] = "stream:error"
  else return
  end
  --assert(st.xmlns == "http://etherx.jabber.org/streams")
  st.xmlns = nil
end

return function(to)
  local frames = {}
  local function SendStanza(st)
    local buf = {}
    xml.Encode(st, buf)
    frames[#frames+1] = table.concat(buf)
  end
  return {
    OpenStream = function()
      SendStanza {[0]="open", xmlns=xmlns, to=to, version="1.0"}
    end,
    SendStanza = SendStanza,
    IsEmpty = function() return #frames == 0 end
    Flush = function()
      -- TODO: do something with js here
    end,
  }
end
