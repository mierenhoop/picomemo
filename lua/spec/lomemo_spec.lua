local lomemo = require"lomemo"

-- We premake the stores, it takes 0.08 sec/store which adds up quick
local store1bin, store2bin = io.open("../o/store1.bin"):read"*a", io.open("../o/store1.bin"):read"*a"

describe("lomemo", function()
  describe("store", function()
    it("can deserialize", function()
      local store = assert.truthy(lomemo.DeserializeStore(store1bin))
      assert.has_error(function() lomemo.DeserializeStore(store1bin.." ") end)
      assert.has_error(function() lomemo.DeserializeStore(store1bin:sub(2)) end)
      assert.has_error(function() lomemo.DeserializeStore(store1bin:sub(1,-2)) end)
      assert.same(store1bin, store:Serialize())
    end)
  end)

  describe("session", function()
    -- TODO: split up 
    it("works", function()
      local store1 = assert.truthy(lomemo.DeserializeStore(store1bin))
      local store2 = assert.truthy(lomemo.DeserializeStore(store2bin))
      local bundle = store1:GetBundle()
      local pk = bundle.prekeys[40]
      bundle.pk = pk.pk
      bundle.pk_id = pk.id
      bundle.prekeys = nil
      -- TODO: speed this function up by using optimized edsign_verify
      local session2 = assert.truthy(lomemo.InitFromBundle(store2, bundle))
      local ser = session2:Serialize()
      assert.same(lomemo.DeserializeSession(ser):Serialize(), ser)
      local msg = "Hello there!"
      local enc, key, iv = assert.truthy(lomemo.EncryptMessage(msg))
      local enckey, isprekey = assert.truthy(session2:EncryptKey(store2, key))
      local session1 = lomemo.NewSession()
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

      local deckey = session1:DecryptKey(store1, isprekey, enckey, sessioncbs)

      assert.same(deckey, key)
      assert.same(lomemo.DecryptMessage(enc, deckey, iv), msg)
    end)
  end)
end)
