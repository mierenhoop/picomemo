#!/usr/bin/env lua5.4

local lomemo = require"lomemo"

local store = lomemo.SetupStore()
local ser = store:Serialize()
assert(lomemo.DeserializeStore(ser):Serialize() == ser)
local store2 = lomemo.SetupStore()

local bundle = store:GetBundle()
local pk = bundle.prekeys[math.random(1,#bundle.prekeys)]
bundle.pk_id = pk.id
bundle.pk = pk.pk
bundle.prekeys = nil

local session2 = assert(lomemo.InitFromBundle(store2, bundle))

ser = session2:Serialize()
assert(lomemo.DeserializeSession(ser):Serialize() == ser)

local msg = "Hello there!"
local enc, key, iv = assert(lomemo.EncryptMessage(msg))

local enckey, isprekey = assert(session2:EncryptKey(store2, key))
enckey, isprekey = assert(session2:EncryptKey(store2, key))

local session = lomemo.NewSession()

local skippedkeys = {}

-- You should use sqlite here, we are using a table for testing. In
-- sqlite you first create a transaction and commit after DecryptKey()
-- succeeds
local sessioncbs = {}
function sessioncbs:load(dh, nr)
  -- Return mk if found and nil if not
  return skippedkeys[dh..nr]
end
function sessioncbs:store(dh, nr, mk, n)
  skippedkeys[dh..nr] = mk
  return true
end

local deckey = session:DecryptKey(store, isprekey, enckey, sessioncbs)

assert(deckey == key)
assert(lomemo.DecryptMessage(enc, deckey, iv) == msg)
print("All tests passed")
