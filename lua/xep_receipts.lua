local util = require"util"
local Q = util.Q

local xmlns = "urn:xmpp:receipts"

return function(session)
  local enabled = true
  return {
    nsfilter = xmlns,
    OnGotStanza = function(st)
      if not enabled then return end
      if Q(st, "request", xmlns) then
        session.SendStanza {[0]="message",
          from=session.GetFullJid(),
          id=session.GenerateId(),
          to=st.from,
          {[0]="received",xmlns=xmlns,id=st.id}
        }
      end
    end,
    AddRequest = function(st, cb)
      st[#st+1] = {[0]="request",xmlns=xmlns}
      session.HookId(assert(st.id), function(st)
        if Q(st, "received", xmlns) then cb() end
      end)
    end,
    Toggle = function(b) enabled = b end,
  }
end
