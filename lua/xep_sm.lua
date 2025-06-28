local xmlns = "urn:xmpp:sm:3"

return function(session)
  local sentenable
  local enabled
  local sent, received = 0, 0
  local shouldreq = false
  return {
    OnFeatures = function(features)
      for _, feat in ipairs(features) do
        if type(feat) == "table" and feat[0] == "sm" and feat.xmlns == xmlns then
          session.SendStanza{[0]="enable",xmlns=xmlns,resume="true"}
          sentenable = true
          break
        end
      end
    end,
    OnDrain = function()
      if shouldreq then
        session.SendStanza{[0]="r",xmlns=xmlns}
        shouldreq = false
      end
    end,
    -- TODO: hook on xmlns
    -- TODO: do something with <a/>
    OnGotStanza = function(st)
      if st.xmlns ~= xmlns then
        received=received+1
        return
      end
      if enabled and st[0] == "r" then
        session.SendStanza{[0]="a",xmlns=xmlns,h=received}
      elseif sentenable and st[0] == "enabled" then
        enabled = true
      end
    end,
    OnSendStanza = function(st)
      if st.xmlns ~= xmlns then shouldreq = sentenable end
      if not sentenable or st.xmlns == xmlns then return end
      sent=sent+1
    end,
  }
end
