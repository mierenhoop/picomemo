local xmlns = "urn:xmpp:ping"
return function(session)
  local pings = {}
  return {
    OnGotStanza = function(st)
      if st.id and pings[st.id] then
        assert(st[0] == "iq")
        assert(st.type == "result")
        if type(pings[st.id]) == "function" then
          pings[st.id]()
        end
        pings[st.id] = nil
      end
    end,
    SendPing = function(cb)
      local id = session.GenerateId()
      pings[id] = cb or true
      session.SendStanza{
        [0]="iq",
        to = "localhost",
        id=id,
        type="get",
        {[0]="ping",xmlns=xmlns},
      }
      session.Drain()
    end,
  }
end
