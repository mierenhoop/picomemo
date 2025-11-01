let assert = require('node:assert');
let omemo = require("../o/omemo.js")

function malloc(n) {
  let p = omemo._malloc(n)
  if (p == 0) throw "malloc fail"
  console.log("Alloced: " + n)
  return p
}

function calloc(n) {
  let p = malloc(n)
  omemo.HEAPU8.fill(0, p, p+n)
  return p
}

function handleRet(r) {
  if (r != 0) throw "omemo error: " + r
}

function getSlice(p, n) {
  return omemo.HEAPU8.slice(p, p+n)
}

function toSerKey(p) {
  // We can do this because p > 0 as null pointer is invalid
  let b = getSlice(p-1, 33)
  b[0] = 5
  return b
}

class Store {
  constructor() {
    this.store = calloc(omemo._get_storesize())
  }

  setup() { handleRet(omemo._omemoSetupStore(this.store)) }

  deserialize(ser) {
    malloced({
      s: ser
    }, ({s}) => {
      handleRet(omemo._omemoDeserializeStore(s, ser.length, this.store))
    })
  }

  serialize() {
    return malloced({
      d: omemo._omemoGetSerializedStoreSize(this.store)
    }, ({d}, resolve) => {
      omemo._omemoSerializeStore(d, this.store)
      return resolve(d)
    })
  }

  getBundle() {
    let n = omemo._get_numprekeys()
    let prekeys = []
    for (let i = 0; i < n; i++) {
      let id = omemo._get_store_pk_id(this.store, i)
      if (id != 0) prekeys.push({
        id,
        key: toSerKey(omemo._get_store_pk(this.store, i)),
      })
    }
    return {
      ik:   toSerKey(omemo._get_store_ik(this.store)),
      spk:  toSerKey(omemo._get_store_spk(this.store)),
      spks: getSlice(omemo._get_store_spks(this.store), 64),
      spk_id: omemo._get_store_spk_id(this.store),
      prekeys,
    }
  }

  free() { omemo._free(this.store) }
}

// TODO: store in session?
class Session {
  constructor() {
    this.session = calloc(omemo._get_sessionsize())
  }

  deserialize(ser) {
    malloced({
      s: ser
    }, ({s}) => {
      handleRet(omemo._omemoDeserializeSession(s, ser.length, this.session))
    })
  }

  serialize() {
    return malloced({
      d: omemo._omemoGetSerializedSessionSize(this.session)
    }, ({d}, resolve) => {
      omemo._omemoSerializeSession(d, this.session)
      return resolve(d)
    })
  }

  initiateSession(store, {
    spks, spk, ik, pk, spk_id, pk_id,
  }) {
    malloced({spks,spk,ik,pk},({spks,spk,ik,pk})=> {
      handleRet(omemo._omemoInitiateSession(this.session, store.store,
        spks, spk, ik, pk, spk_id, pk_id
      ))
    })
  }

  encryptKey(store, key) {
    return malloced({
      msg: omemo._get_keymessagesize(),
      k: key,
    }, ({msg,k}) => {
      handleRet(omemo._omemoEncryptKey(this.session, store.store, msg, k, key.length))
      return {
        msg: getSlice(omemo._get_km_p(msg),
                      omemo._get_km_n(msg)),
        prekey: omemo._get_km_ispk(msg) != 0,
      }
    })
  }

  decryptKey(store, prekey, msg) {
    // for oldmemo
    let keysize = 32
    return malloced({
      msgp: msg,
      key: keysize,
      // Reserve 8 bytes for out pointer in case sizeof size_t == 8
      // It's little endian so it's no problem either way (4 vs 8)
      keysizep: 8,
    }, ({msgp, key, keysizep}, resolve) => {
      omemo.HEAPU8[keysizep] = keysize
      handleRet(omemo._omemoDecryptKey(this.session, store.store, key, keysizep, prekey ? 1 : 0, msgp, msg.length))
      // TODO: do something with new keysize
      return resolve(key)
    })
  }

  // You probaly don't have to free a Session or Store at all because in the
  // intended usecase you rarely destroy sessions anyways
  free() { omemo._free(this.session) }
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
    // TODO: we can put all in one malloc
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

function getRandomPrekey(prekeys) {
  return prekeys.length > 0 ?
    prekeys[(Math.random()*prekeys.length) | 0] : null
}

omemo.onRuntimeInitialized = () => {
  let store1 = new Store()
  let store2 = new Store()
  store1.setup()
  store2.setup()

  let session1 = new Session()
  let session2 = new Session()

  let bundle = store1.getBundle()
  let pk = getRandomPrekey(bundle.prekeys)
  delete bundle.prekeys
  bundle.pk = pk.key
  bundle.pk_id = pk.id

  session2.initiateSession(store2, bundle)

  let plain = "Hello"
  let enc = encryptMessage(toBuf("Hello"))
  let {prekey, msg} = session2.encryptKey(store2, enc.key)
  let key = session1.decryptKey(store1, prekey, msg)
  //store.free()

  assert.equal(new TextDecoder().decode(decryptMessage(key, enc.iv, enc.payload)), plain)

  console.log("Test successful")
}
