-- TODO: should it be possible for the user to disable resumable

resumable = false
enabled = false
streamid = nil

-- After negotiation
function Init()
  Send[[<enable xmlns="urn:xmpp:sm:3" resume="true"/>]]
end

-- Received stanza with xmlns=current
function Receive(stanza)
  if stanza[0] == "enabled" then
    assert(not enabled)
    enabled = true
    resumable = stanza.resume == "true" or stanza.resume == "1"
  elseif stanza[0] == "resumed" then

  else
    error"unexpected stanza"
  end
end

function Resume()

end
