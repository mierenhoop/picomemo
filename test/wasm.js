let assert = require('node:assert');
let omemo = require("../o/omemo.js")


function handleRet(r) {
  if (r != 0) throw "omemo error: " + r
}

class Store {
  constructor() {
    this.store = omemo._AllocateStore()
    if (this.store == 0) throw "malloc fail"
  }

  setup() { handleRet(omemo._omemoSetupStore(this.store)) }

  free() { omemo._free(this.store) }
}

class Session {
  constructor() {
    this.session = omemo._AllocateSession()
    if (this.session == 0) throw "malloc fail"
  }

  free() { omemo._free(this.session) }
}

function malloc(n) {
  let p = omemo._malloc(n)
  if (p == 0) throw "malloc fail"
  console.log("Alloced: " + n)
  return p
}

function toBuf(b) {
  if (typeof b == "string")
    b = new TextEncoder().encode(b)
  else if (!b instanceof Uint8Array)
    throw "expected Uint8Array"
  return b
}

function toHeap(b) {
  let p = malloc(b.length)
  omemo.HEAPU8.set(b, p)
  return p
}

function malloced(m, cb) {
  let allocs = {}
  let pton = {}
  let resolve = p => omemo.HEAPU8.slice(p,p+pton[p])
  try {
    for (let [k,v] of Object.entries(m)) {
      let [p,n] = (typeof v == "number") ?
        [malloc(v),v] : [toHeap(v),v.length]
      allocs[k] = p
      pton[p] = n
    }
    return cb(allocs, resolve)
  } finally {
    for (let p of Object.values(allocs)) {
      omemo._free(p)
      console.log("Freed: " + pton[p])
    }
  }
}

// oldmemo
/**
 * @param payload Uint8Array
 * @returns {
 *   key: Uint8Array,
 *   iv: Uint8Array,
 *   payload: Uint8Array,
 * }
 */
function encryptMessage(payload) {
  return malloced({
    key: 32,
    iv: 12,
    enc: payload.length,
    pay: payload,
  }, ({key,iv,enc,pay}, resolve) => {
    handleRet(omemo._omemoEncryptMessage(enc, key, iv, pay, payload.length))
    return {
      key: resolve(key),
      iv: resolve(iv),
      payload: resolve(enc),
    }
  })
}

function decryptMessage(key, iv, payload) {
  return malloced({
    key,
    iv,
    dec: payload.length,
    pay: payload,
  }, ({key:keyp,iv,dec,pay}, resolve) => {
    handleRet(omemo._omemoDecryptMessage(dec, keyp, key.length, iv, pay, payload.length))
    return resolve(dec)
  })
}


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

  let enc = encryptMessage(toBuf("Hello"))
  console.log(enc)
  console.log(new TextDecoder().decode(decryptMessage(enc.key, enc.iv, enc.payload)))

  console.log("Test successful")
}
