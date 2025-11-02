let assert = require("node:assert");
let omemo = require("../o/omemo.js")
let crypto = require("node:crypto")

function malloc(n) {
  let p = omemo._malloc(n)
  if (p == 0) throw "malloc fail"
  //console.log("Alloced: " + n)
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
    this.ptr = calloc(omemo._get_storesize())
  }

  setup() { handleRet(omemo._omemoSetupStore(this.ptr)) }

  deserialize(ser) {
    malloced({
      s: ser
    }, ({s}) => {
      handleRet(omemo._omemoDeserializeStore(s, ser.length, this.ptr))
    })
  }

  serialize() {
    return malloced({
      d: omemo._omemoGetSerializedStoreSize(this.ptr)
    }, ({d}, resolve) => {
      omemo._omemoSerializeStore(d, this.ptr)
      return resolve(d)
    })
  }

  getBundle() {
    let n = omemo._get_numprekeys()
    let prekeys = []
    for (let i = 0; i < n; i++) {
      let id = omemo._get_store_pk_id(this.ptr, i)
      if (id != 0) prekeys.push({
        id,
        key: toSerKey(omemo._get_store_pk(this.ptr, i)),
      })
    }
    return {
      ik:   toSerKey(omemo._get_store_ik(this.ptr)),
      spk:  toSerKey(omemo._get_store_spk(this.ptr)),
      spks: getSlice(omemo._get_store_spks(this.ptr), 64),
      spk_id: omemo._get_store_spk_id(this.ptr),
      prekeys,
    }
  }

  free() { omemo._free(this.ptr) }
}

class Session {
  constructor(store) {
    this.store = store
    this.ptr = calloc(omemo._get_sessionsize())
  }

  deserialize(ser) {
    malloced({
      s: ser
    }, ({s}) => {
      handleRet(omemo._omemoDeserializeSession(s, ser.length, this.ptr))
    })
  }

  serialize() {
    return malloced({
      d: omemo._omemoGetSerializedSessionSize(this.ptr)
    }, ({d}, resolve) => {
      omemo._omemoSerializeSession(d, this.ptr)
      return resolve(d)
    })
  }

  initiateSession({
    spks, spk, ik, pk, spk_id, pk_id,
  }) {
    malloced({spks,spk,ik,pk},({spks,spk,ik,pk})=> {
      handleRet(omemo._omemoInitiateSession(this.ptr, this.store.ptr,
        spks, spk, ik, pk, spk_id, pk_id
      ))
    })
  }

  encryptKey(key) {
    return malloced({
      msg: omemo._get_keymessagesize(),
      k: key,
    }, ({msg,k}) => {
      handleRet(omemo._omemoEncryptKey(this.ptr, this.store.ptr, msg, k, key.length))
      return {
        msg: getSlice(omemo._get_km_p(msg),
                      omemo._get_km_n(msg)),
        prekey: omemo._get_km_ispk(msg) != 0,
      }
    })
  }

  decryptKey(prekey, msg) {
    // for oldmemo
    let keysize = 32
    return malloced({
      msgp: msg,
      key: keysize,
      // Reserve 8 bytes for out pointer in case sizeof size_t == 8
      // It's little endian so it's no problem either way (4 vs 8)
      keysizep: 8,
    }, ({msgp, key, keysizep}, resolve) => {
      omemo.HEAPU8[omemo._g_triedload] = 0
      let dec = () => {
        omemo.HEAPU8[keysizep] = keysize
        return omemo._omemoDecryptKey(this.ptr, this.store.ptr, key, keysizep, prekey ? 1 : 0, msgp, msg.length)
      }
      let r = dec()
      // TODO: don't hardcode OMEMO_EUSER, also have error enum in JS
      if (r != -7) {
        throw "omemo: unreachable code path"
      }
      let dh = getSlice(omemo._get_mk_dh(omemo._g_loadkey), 32)
      let nr = omemo._get_mk_nr(omemo._g_loadkey)
      let mk = this.loadKey(dh, nr)
      if (mk) {
        malloced({mk},({mk})=>omemo._set_mk_mk(omemo._g_loadkey, mk))
        omemo.HEAPU8[omemo._g_suppliedkey] = 1
      } else {
        omemo.HEAPU8[omemo._g_suppliedkey] = 0
      }
      omemo.HEAPU8[omemo._g_triedload] = 1
      omemo.HEAPU8[omemo._g_skipbuf] = 0
      r = dec()
      if (r == -7) {
        let mksize = omemo._get_messagekeysize()
        let n = omemo._get_nskip()
        malloced({buf: n * mksize}, ({buf}) => {
          omemo._set_skipbuf(buf)
          handleRet(dec())
          let keys = []
          for (let i = 0; i < n; i++) {
            keys.push({
              dh: getSlice(omemo._get_mk_dh(buf+i*mksize), 32),
              mk: getSlice(omemo._get_mk_mk(buf+i*mksize), 32),
              nr: omemo._get_mk_nr(buf+i*mksize),
            })
          }
          this.storeKeys(keys)
        })
      } else {
        handleRet(r)
      }
      return getSlice(key, omemo.HEAPU8[keysizep])
    })
  }

  // You probably don't have to free a Session or Store at all because in the
  // intended usecase you rarely destroy sessions anyways
  free() { omemo._free(this.ptr) }

  // Override these
  loadKey(dh, nr) {}
  storeKeys(keys) {}
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
      //console.log("Freed: " + pton[p])
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
  let enc = encryptMessage(toBuf("Hello"))
  let {prekey, msg} = session2.encryptKey(enc.key)
  let key = session1.decryptKey(prekey, msg)
  //store.free()

  assert.equal(new TextDecoder().decode(decryptMessage(key, enc.iv, enc.payload)), plain)

  console.log("Test successful")
}
