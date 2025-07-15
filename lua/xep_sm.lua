local xmlns = "urn:xmpp:sm:3"

return function(session)
  local sentenable
  local enabled
  local sent, received = 0, 0
  local shouldreq = false
  -- TODO: add timeout
  -- map from sent stanza sequence number (integer) to stanza
  local pending = {}
  return {
    OnFeatures = function(features)
      -- If disabled never send so this whole xep will do nothing
      if session.opts.disablesm then return end
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
    -- TODO: check received <a/>
    OnGotStanza = function(st)
      if not sentenable then return end
      if st.xmlns ~= xmlns then
        if enabled then received=received+1 end
        return
      end
      if st[0] == "r" then
        assert(enabled, "xep_sm: request sent before enabled")
        session.SendStanza{[0]="a",xmlns=xmlns,h=received}
      elseif st[0] == "a" then
        local h = assert(tonumber(st.h), "xep_sm: invalid xml")
        assert(h <= sent, "xep_sm: handled count too high")
        for i = h, 1, -1 do
          if not pending[i] then break end
          pending[i] = nil
        end
        -- TODO: this is a hack for if the gc doesn't ever shrink the table
        -- this resets the table if it's empty
        if not pending[h+1] then pending = {} end
      elseif st[0] == "enabled" then
        enabled = true
      elseif st[0] == "failed" then
        -- TODO: maybe error?
        -- Fully disable this xep
        sentenable = false
      elseif st[0] == "resumed" then
        -- TODO: check if we ever asked for resume
        -- retransmit all pending stanzas
        local h = assert(tonumber(st.h), "xep_sm: invalid xml")
        assert(h <= sent, "xep_sm: handled count too high")
        for i = h+1, sent do
          -- TODO: send stanza without calling hooks...
        end
      end
    end,
    OnSendStanza = function(st)
      if not sentenable then return end
      -- Only send request on drain when a regular stanza is sent
      if st.xmlns ~= xmlns then
        shouldreq = true
        sent=sent+1
        pending[sent] = st
      end
    end,
    Resume = function()
      -- TODO: new session???
    end,
  }
end
