local xml = require"xml"

return function(to)
  local sendbuf = {}
  return {
  OpenStream = function()
    sendbuf = {"<?xml version=\"1.0\"?>"}
    xml.Encode({[0]="stream:stream",
      version = "1.0",
      ["xml:lang"] = "en",
      xmlns = "jabber:client",
      -- TODO: from?
      to = to,
      ["xmlns:stream"] = "http://etherx.jabber.org/streams",
    }, sendbuf)
    -- HACK
    assert(sendbuf[#sendbuf] == "/>")
    sendbuf[#sendbuf] = ">"
  end,
  SendStanza = function(st) xml.Encode(st, sendbuf) end,
  IsEmpty = function() return #sendbuf == 0 end,
  Flush = function()
    Send(table.concat(sendbuf))
    sendbuf = {}
  end,
  }
end
