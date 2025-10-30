let assert = require('node:assert');
let omemo = require("../o/omemo.js")

omemo.onRuntimeInitialized = () => {
  let store = omemo._AllocateStore()
  assert.equal(omemo._omemoSetupStore(store), 0)
  let len = omemo._omemoGetSerializedStoreSize(store)
  let ser = omemo._malloc(len)
  omemo._omemoSerializeStore(ser, store)
  omemo._free(store)
  let store2 = omemo._AllocateStore()
  assert.equal(omemo._omemoDeserializeStore(ser, len, store2), 0)
  omemo._free(ser)
  omemo._free(store2)
}
