/**
 * Friendly OMEMO API for JavaScript
 *
 * WASM overhead makes it about ~5x slower than native x86.
 */

import Module from "./o/wasm.js"
let omemo = await Module()

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

export class Store {
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

export class Session {
  maxSkip = 100

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
        if (n > this.maxSkip) throw "omemo: skip too many keys"
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
/*
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
}*/

export async function encryptMessage(payload) {
  let crypkey = await crypto.subtle.generateKey(
    { name: "AES-GCM", length: 128 },
    true,
    ["encrypt"],
  )
  let iv = crypto.getRandomValues(new Uint8Array(12))
  let opts = { name: "AES-GCM", iv }
  let enc = await crypto.subtle.encrypt(opts, crypkey, payload)
  enc = new Uint8Array(enc)
  crypkey = new Uint8Array(await crypto.subtle.exportKey("raw", crypkey))
  let key = new Uint8Array(32)
  key.set(crypkey)
  key.set(enc.slice(enc.length-16), 16)
  return {
    key,
    iv,
    payload: enc.slice(0, enc.length - 16),
  }
}

export async function decryptMessage(key, iv, payload) {
  let crypkey = await crypto.subtle.importKey(
    "raw",
    key.slice(0,16),
    "AES-GCM",
    false,
    ["decrypt"],
  )
  let pay = new Uint8Array(payload.length + 16)
  pay.set(payload)
  pay.set(key.slice(16,32), payload.length)
  let opts = { name: "AES-GCM", iv }
  return await crypto.subtle.decrypt(opts, crypkey, pay)
}
