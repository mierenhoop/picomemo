#!/usr/bin/env lua5.4
require"native"

local NewSession = require"session"

Connect("localhost", "localhost", "5222")
local session = NewSession({
  localpart = "admin",
  domainpart = "localhost",
  resourcepart = "testres",
  usetls = true,
  saslmech = "PLAIN",
  password = "adminpass",
})

function OnStdin()
  local msg = io.read("*l")
  if msg == "" then return end
  if msg == "ping" then
    session.xep_ping.SendPing(function()
      print"Got pong"
    end)
    return
  end
  if session.IsReady() then
    session.SendStanza {[0]="message",
      id=session.GenerateId(),
      to="user@localhost",
      type="chat",
      ["xml:lang"]="en",
      {[0]="body", msg },
    }
  end
end

function OnReceive(data)
  session.FeedStream(data)
end

EventLoop()
