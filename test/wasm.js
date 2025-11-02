import * as assert from "node:assert";
import { Session, Store, encryptMessage, decryptMessage } from "../o/omemo.min.js"

function toBuf(b) {
  if (typeof b == "string")
    b = new TextEncoder().encode(b)
  else if (!(b instanceof Uint8Array))
    throw "expected Uint8Array"
  return b
}

function getRandomPrekey(prekeys) {
  return prekeys.length > 0 ?
    prekeys[(Math.random()*prekeys.length) | 0] : null
}

function hashmk(dh, nr) {
  return dh.toString()+nr
}

class MySession extends Session {
  keys = new Map
  loadKey(dh, nr) {
    let mk = this.keys.get(hashmk(dh, nr))
    return mk
  }
  storeKeys(keys) {
    for (let {dh,nr,mk} of keys) {
      this.keys.set(hashmk(dh, nr), mk)
    }
  }
}

let store1 = new Store()
let store2 = new Store()
store1.setup()
store2.setup()

let session1 = new MySession(store1)
let session2 = new MySession(store2)

let bundle = store1.getBundle()
let pk = getRandomPrekey(bundle.prekeys)
delete bundle.prekeys
bundle.pk = pk.key
bundle.pk_id = pk.id

session2.initiateSession(bundle)

let msgs = []

let send = (s, id) => {
  // TODO: don't hardcode 32 (OMEMO_KEYSIZE)
  let key = crypto.getRandomValues(new Uint8Array(32))
  msgs[id] = s.encryptKey(key)
  msgs[id].key = key
}

let recv = (s, id) => {
  let key = s.decryptKey(msgs[id].prekey, msgs[id].msg)
  assert.equal(key.toString(), msgs[id].key.toString())
}

send(session2, 0)
recv(session1, 0)

send(session1, 1)
recv(session2, 1)

send(session1, 2)
recv(session2, 2)

send(session1, 3)
send(session1, 4)

recv(session2, 4)

recv(session2, 3)

let plain = "Hello"
encryptMessage(toBuf(plain)).then(enc=>{
  let {prekey, msg} = session2.encryptKey(enc.key)
  let key = session1.decryptKey(prekey, msg)
  decryptMessage(key, enc.iv, enc.payload).then(o=>{
    assert.equal(new TextDecoder().decode(o), plain)
    console.log("Test successful")
  })
})
