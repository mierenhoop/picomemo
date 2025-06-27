local xmlns = "urn:xmpp:sm:3"

return function(session)
  local sentenable
  local enabled
  local sent, received = 0, 0
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
      if enabled then session.SendStanza{[0]="r",xmlns=xmlns} end
    end,
    -- TODO: hook on xmlns
    OnGotStanza = function(st)
      if st.xmlns ~= xmlns then
        received=received+1
        return
      end
      if enabled and st[0] == "r" then
        session.SendStanza{[0]="a",xmlns=xmlns,h=sent}
      elseif sentenable and st[0] == "enabled" then
        enabled = true
      end
    end,
    OnSendStanza = function(st)
      if not enabled or st.xmlns == xmlns then return end
      sent=sent+1
    end,
  }
end
