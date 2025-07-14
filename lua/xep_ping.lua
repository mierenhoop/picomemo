local util = require"util"
local Q = util.Q

local xmlns = "urn:xmpp:ping"

local ErrValid = "xep_ping: invalid response"

return function(session)
  local enabled = true
  return {
    nsfilter = xmlns,
    OnInit = function()
      if session.xep_disco then session.xep_disco.Register(xmlns) end
    end,
    OnGotStanza = function(st)
      -- TODO: service-unavailable?
      if not enabled then return end
      if Q(st, "ping", xmlns) then
        session.SendStanza {[0]="iq",
          type="result",
          id=session.GenerateId(),
          from=session.GetFullJid(),
          to=st.from,
        }
      end
    end,
    SendPing = function(cb)
      local id = session.HookId(function(st)
        assert(st[0] == "iq", ErrValid)
        assert(st.type == "result", ErrValid)
        if cb then cb() end
      end)
      session.SendStanza{
        [0]="iq",
        to = "localhost",
        id=id,
        type="get",
        {[0]="ping",xmlns=xmlns},
      }
    end,
    Toggle = function(b) enabled = b end,
  }
end
