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
  disablesm = true,
})


local useomemo
function OnStdin()
  local msg = io.read("*l")
  if msg == "" then return end
  if msg == "ping" then
    session.xep_ping.SendPing(function()
      print"Got pong"
    end)
    return
  end
  if msg == "/omemo" then
    useomemo = true
    return
  end
  if session.IsReady() then
    if not useomemo then
      session.SendStanza {[0]="message",
        id=session.GenerateId(),
        to="user@localhost",
        type="chat",
        ["xml:lang"]="en",
        {[0]="body", msg },
      }
    else
      session.xep_omemo.SendMessage(msg, "user@localhost")
    end
  end
end

local done
function OnReceive(data)
  session.FeedStream(data)
  if not done and session.IsReady() then
    session.xep_omemo.SendMessage("Hello", "user@localhost")
    done = true
  end
end

EventLoop()
