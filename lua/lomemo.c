/**
 * Copyright 2025 mierenhoop
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <sys/random.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "omemo.h"

#if LUA_VERSION_NUM > 501
#define luaL_openlib(L, name, reg, nup) luaL_setfuncs(L, reg, nup)
#endif

#define CheckLStringFix(idx, fix, err) CheckLStringFix(L, idx, fix, "omemo: " err " size wrong")
const char *(CheckLStringFix)(lua_State *L, int idx, int fix, const char *err) {
  size_t n;
  const char *s = luaL_checklstring(L, idx, &n);
  if (n != (fix)) luaL_argerror(L, idx, err);
  return s;
}


struct Session {
  struct omemoSession s;
  // Wrap so that in Load/Store MessageKey we can callback to Lua
  lua_State *L;
};

int omemoRandom(void *d, size_t n) {
  assert(getrandom(d, n, 0) == n);
  return 0;
}

int omemoLoadMessageKey(struct omemoSession *_session, struct omemoMessageKey *k) {
  struct Session *session = (struct Session*)_session;
  lua_State *L = session->L;
  lua_getfield(L, 5, "load");
  lua_pushvalue(L, 1);
  lua_pushlstring(L, k->dh, sizeof(k->dh));
  lua_pushinteger(L, k->nr);
  lua_call(L, 3, 1);
  if (lua_isstring(L, -1)) {
    memcpy(k->mk, CheckLStringFix(-1, sizeof(k->mk), "message key"), sizeof(k->mk));
    lua_pop(L, 1);
    return 0;
  }
  return 1;
}

int omemoStoreMessageKey(struct omemoSession *_session, const struct omemoMessageKey *k, uint64_t n) {
  struct Session *session = (struct Session*)_session;
  lua_State *L = session->L;
  // TODO: check error?
  lua_getfield(L, 5, "store");
  lua_pushvalue(L, 1);
  lua_pushlstring(L, k->dh, sizeof(k->dh));
  lua_pushinteger(L, k->nr);
  lua_pushlstring(L, k->mk, sizeof(k->mk));
  lua_pushinteger(L, n);
  lua_call(L, 5, 1);
  int r = lua_toboolean(L, -1);
  lua_pop(L, 1);
  return r ? 0 : OMEMO_EUSER;
}

void *Alloc(lua_State *L, size_t n) {
  void *p;
  if (!(p = malloc(n))) luaL_error(L, "Allocation failed");
  return p;
}

static int SetupStore(lua_State *L) {
  struct omemoStore *store = lua_newuserdata(L, sizeof(struct omemoStore));
  luaL_getmetatable(L, "lomemo.Store");
  lua_setmetatable(L, -2);
  if (omemoSetupStore(store))
    luaL_error(L, "setup store");
  return 1;
}

static void CopySizedField(lua_State *L, int i, const char *f, int n, uint8_t *d, bool hastype) {
  lua_getfield(L, i, f);
  // TODO: error message is  wrong when this fails
  size_t len;
  const char *s = luaL_checklstring(L, -1, &len);
  if (len == n+hastype)
    memcpy(d, s+hastype, n), lua_pop(L, 1);
  else
    lua_pop(L, 1), luaL_argerror(L, i, "field not right size");
}

static int NewSession(lua_State *L) {
  struct Session *session = lua_newuserdata(L, sizeof(struct Session));
  luaL_getmetatable(L, "lomemo.Session");
  lua_setmetatable(L, -2);
  // TODO: is this needed?
  memset(session, 0, sizeof(struct Session));
  return 1;
}

static int InitFromBundle(lua_State *L) {
  struct omemoStore *store = luaL_checkudata(L, 1, "lomemo.Store");
  struct Session *session = lua_newuserdata(L, sizeof(struct Session));
  luaL_getmetatable(L, "lomemo.Session");
  lua_setmetatable(L, -2);
  struct omemoBundle bundle = {0};
#define C(ht, f) CopySizedField(L, 2, #f, sizeof(bundle.f), bundle.f, ht)
  C(0, spks);
  C(1, spk);
  C(1, ik);
  C(1, pk);
  lua_getfield(L, 2, "spk_id");
  lua_Integer spk_id = luaL_checkinteger(L, -1);
  lua_pop(L, 1);
  if (spk_id < 0 || spk_id > UINT32_MAX) luaL_argerror(L, 2, "0 <= spk_id <= UINT32_MAX");
  lua_getfield(L, 2, "pk_id");
  lua_Integer pk_id = luaL_checkinteger(L, -1);
  lua_pop(L, 1);
  if (pk_id < 0 || pk_id > UINT32_MAX) luaL_argerror(L, 2, "0 <= pk_id <= UINT32_MAX");
  bundle.pk_id = pk_id;
  bundle.spk_id = spk_id;
  int r = omemoInitFromBundle(&session->s, store, &bundle);
  if (!r) {
    return 1;
  } else {
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_pushstring(L, "error");
    return 2;
  }
}

static int DecryptKey(lua_State *L) {
  size_t n;
  struct Session *session = luaL_checkudata(L, 1, "lomemo.Session");
  struct omemoStore *store = luaL_checkudata(L, 2, "lomemo.Store");
  // TODO: check for nil?
  bool isprekey = lua_toboolean(L, 3);
  const char *s = luaL_checklstring(L, 4, &n);
  omemoKeyPayload deckey;
  session->L = L;
  int r = omemoDecryptKey(&session->s, store, deckey, isprekey, s, n); if (!r) {
    lua_pushlstring(L, deckey, sizeof(deckey));
    return 1;
  } else {
    lua_pushnil(L);
    lua_pushstring(L, "lomemo: decrypt key failed");
    return 2;
  }
}

static int EncryptKey(lua_State *L) {
  struct Session *session = luaL_checkudata(L, 1, "lomemo.Session");
  struct omemoStore *store = luaL_checkudata(L, 2, "lomemo.Store");
  // TODO: check for nil?
  const char *s = CheckLStringFix(3, sizeof(omemoKeyPayload), "key");
  struct omemoKeyMessage enc;
  int r = omemoEncryptKey(&session->s, store, &enc, s);
  if (!r) {
    lua_pushlstring(L, enc.p, enc.n);
    lua_pushboolean(L, enc.isprekey);
    return 2;
  } else {
    lua_pushnil(L);
    lua_pushstring(L, "lomemo: decrypt key failed");
    return 2;
  }
}

static int DeserializeSession(lua_State *L) {
  size_t n;
  const char *s = luaL_checklstring(L, 1, &n);
  struct Session *session = lua_newuserdata(L, sizeof(struct Session));
  if (omemoDeserializeSession(s, n, &session->s))
    luaL_error(L, "lomemo: deserializing protobuf");
  luaL_getmetatable(L, "lomemo.Session");
  lua_setmetatable(L, -2);
  return 1;
}

static int SerializeSession(lua_State *L) {
  struct Session *session = luaL_checkudata(L, 1, "lomemo.Session");
  size_t n = omemoGetSerializedSessionSize(&session->s);
  uint8_t *p = Alloc(L, n);
  omemoSerializeSession(p, &session->s);
  lua_pushlstring(L, p, n);
  free(p);
  return 1;
}

static int DeserializeStore(lua_State *L) {
  size_t n;
  const char *s = luaL_checklstring(L, 1, &n);
  struct omemoStore *store = lua_newuserdata(L, sizeof(struct omemoStore));
  if (omemoDeserializeStore(s, n, store))
    luaL_error(L, "lomemo: deserializing protobuf");
  luaL_getmetatable(L, "lomemo.Store");
  lua_setmetatable(L, -2);
  return 1;
}

static int SerializeStore(lua_State *L) {
  struct omemoStore *store = luaL_checkudata(L, 1, "lomemo.Store");
  size_t n = omemoGetSerializedStoreSize(store);
  uint8_t *p = Alloc(L, n);
  omemoSerializeStore(p, store);
  lua_pushlstring(L, p, n);
  free(p);
  return 1;
}

static int GetBundle(lua_State *L) {
  omemoSerializedKey spk, ik;
  struct omemoStore *store = luaL_checkudata(L, 1, "lomemo.Store");
  lua_newtable(L);
  lua_pushlstring(L, store->cursignedprekey.sig, 64);
  lua_setfield(L, -2, "spks");
  omemoSerializeKey(spk, store->cursignedprekey.kp.pub);
  lua_pushlstring(L, spk, sizeof(spk));
  lua_setfield(L, -2, "spk");
  lua_pushinteger(L, store->cursignedprekey.id);
  lua_setfield(L, -2, "spk_id");
  omemoSerializeKey(ik, store->identity.pub);
  lua_pushlstring(L, ik, sizeof(ik));
  lua_setfield(L, -2, "ik");
  lua_newtable(L);
  int p = 0;
  for (int i = 0; i < OMEMO_NUMPREKEYS; i++) {
    if (!store->prekeys[i].id) continue;
    omemoSerializedKey pk;
    omemoSerializeKey(pk, store->prekeys[i].kp.pub);
    lua_newtable(L);
    lua_pushlstring(L, pk, sizeof(pk));
    lua_setfield(L, -2, "pk");
    lua_pushinteger(L, store->prekeys[i].id);
    lua_setfield(L, -2, "id");
    lua_rawseti(L, -2, ++p);
  }
  lua_setfield(L, -2, "prekeys");
  return 1;
}

static int RefillPreKeys(lua_State *L) {
  struct omemoStore *store = luaL_checkudata(L, 1, "lomemo.Store");
  int r = omemoRefillPreKeys(store);
  if (r) luaL_error(L, "lomemo: refill prekeys failed");
  return 0;
}

static int RotateSignedPreKey(lua_State *L) {
  struct omemoStore *store = luaL_checkudata(L, 1, "lomemo.Store");
  int r = omemoRotateSignedPreKey(store);
  if (r) luaL_error(L, "lomemo: rotate signed prekey failed");
  return 0;
}

static int EncryptMessage(lua_State *L) {
  size_t msgn;
  const char *msg = luaL_checklstring(L, 1, &msgn);
  uint8_t iv[12];
  omemoKeyPayload key;
  char *enc = Alloc(L, msgn);
  int r = omemoEncryptMessage(enc, key, iv, msg, msgn);
  if (!r) {
    lua_pushlstring(L, enc, msgn);
    lua_pushlstring(L, key, sizeof(key));
    lua_pushlstring(L, iv, sizeof(iv));
    free(enc);
    return 3;
  } else {
    free(enc);
    lua_pushnil(L);
    lua_pushstring(L, "lomemo: encrypt message failed");
    return 2;
  }
}

static int DecryptMessage(lua_State *L) {
  size_t msgn;
  const char *msg = luaL_checklstring(L, 1, &msgn);
  const char *key = CheckLStringFix(2, sizeof(omemoKeyPayload), "key");
  const char *iv = CheckLStringFix(3, 12, "iv");
  char *dec = Alloc(L, msgn);
  int r = omemoDecryptMessage(dec, key, sizeof(omemoKeyPayload), iv, msg, msgn);
  if (!r) {
    lua_pushlstring(L, dec, msgn);
    free(dec);
    return 1;
  } else {
    free(dec);
    lua_pushnil(L);
    lua_pushstring(L, "lomemo: encrypt message failed");
    return 2;
  }
}

static const luaL_Reg lib[] = {
  {"SetupStore", SetupStore},
  {"DeserializeStore", DeserializeStore},
  {"DeserializeSession", DeserializeSession},
  {"NewSession", NewSession}, // Use this when receiving first message
  {"InitFromBundle", InitFromBundle}, // Use this when sending first message
  {"EncryptMessage", EncryptMessage},
  {"DecryptMessage", DecryptMessage},
  {NULL,NULL},
};

static const luaL_Reg storemt[] = {
  {"Serialize",SerializeStore},
  {"GetBundle",GetBundle},
  {"RefillPreKeys", RefillPreKeys},
  {"RotateSignedPreKey", RotateSignedPreKey},
  {NULL,NULL},
};

static const luaL_Reg sessionmt[] = {
  {"DecryptKey",DecryptKey},
  {"EncryptKey",EncryptKey},
  {"Serialize",SerializeSession},
  {NULL,NULL},
};

static void RegisterMetatable(lua_State *L, const luaL_Reg *lib, const char *name) {
  luaL_newmetatable(L, name);
  lua_pushliteral(L, "__index");
  lua_pushvalue(L, -2);
  lua_rawset(L, -3);
  luaL_openlib(L, NULL, lib, 0);
  lua_pop(L, 1);
}

int luaopen_lomemo(lua_State *L) {
  lua_newtable(L);
  luaL_openlib(L, NULL, lib, 0);
  RegisterMetatable(L, storemt, "lomemo.Store");
  RegisterMetatable(L, sessionmt, "lomemo.Session");
  return 1;
}
