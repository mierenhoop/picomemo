#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <sys/random.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "omemo.h"

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
    if (lua_rawlen(L, -1) != sizeof(k->mk))
      luaL_error(L, "omemo: message key size");
    memcpy(k->mk, luaL_checkstring(L, -1), sizeof(k->mk));
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
  // TODO: other code than 1
  return r ? 0 : 1;
}

void *Alloc(lua_State *L, size_t n) {
  void *p;
  if (!(p = malloc(n))) luaL_error(L, "Allocation failed");
  return p;
}

static int SetupStore(lua_State *L) {
  struct omemoStore *store = lua_newuserdatauv(L, sizeof(struct omemoStore), 0);
  luaL_setmetatable(L, "lomemo.Store");
  if (omemoSetupStore(store))
    luaL_error(L, "setup store");
  return 1;
}

static void CopySizedField(lua_State *L, int i, const char *f, int n, uint8_t *d, bool hastype) {
  lua_getfield(L, i, f);
  // TODO: error message is  wrong when this fails
  const char *s = luaL_checkstring(L, -1);
  if (lua_rawlen(L, -1) == n+hastype)
    memcpy(d, s+hastype, n), lua_pop(L, 1);
  else
    lua_pop(L, 1), luaL_argerror(L, i, "field not right size");
}

static int NewSession(lua_State *L) {
  struct Session *session = lua_newuserdatauv(L, sizeof(struct Session), 0);
  luaL_setmetatable(L, "lomemo.Session");
  // TODO: is this needed?
  memset(session, 0, sizeof(struct Session));
  return 1;
}

static int InitFromBundle(lua_State *L) {
  struct omemoStore *store = luaL_checkudata(L, 1, "lomemo.Store");
  struct Session *session = lua_newuserdatauv(L, sizeof(struct Session), 0);
  luaL_setmetatable(L, "lomemo.Session");
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
  struct Session *session = luaL_checkudata(L, 1, "lomemo.Session");
  struct omemoStore *store = luaL_checkudata(L, 2, "lomemo.Store");
  // TODO: check for nil?
  bool isprekey = lua_toboolean(L, 3);
  const char *s = luaL_checkstring(L, 4);
  size_t n = lua_rawlen(L, 4);
  omemoKeyPayload deckey;
  session->L = L;
  int r = omemoDecryptKey(&session->s, store, deckey, isprekey, s, n);
  if (!r) {
    lua_pushlstring(L, deckey, sizeof(deckey));
    return 1;
  } else {
    lua_pushnil(L);
    lua_pushstring(L, "omemo: decrypt key failed");
    return 2;
  }
}

static int EncryptKey(lua_State *L) {
  struct Session *session = luaL_checkudata(L, 1, "lomemo.Session");
  struct omemoStore *store = luaL_checkudata(L, 2, "lomemo.Store");
  // TODO: check for nil?
  const char *s = luaL_checkstring(L, 3);
  size_t n = lua_rawlen(L, 3);
  if (n != sizeof(omemoKeyPayload)) luaL_argerror(L, 3, "key size not right");
  struct omemoKeyMessage enc;
  int r = omemoEncryptKey(&session->s, store, &enc, s);
  if (!r) {
    lua_pushlstring(L, enc.p, enc.n);
    lua_pushboolean(L, enc.isprekey);
    return 2;
  } else {
    lua_pushnil(L);
    lua_pushstring(L, "omemo: decrypt key failed");
    return 2;
  }
}

static int DeserializeSession(lua_State *L) {
  const char *s = luaL_checkstring(L, 1);
  size_t n = lua_rawlen(L, 1);
  struct Session *session = lua_newuserdatauv(L, sizeof(struct Session), 0);
  if (omemoDeserializeSession(s, n, &session->s))
    luaL_error(L, "omemo: deserializing protobuf");
  luaL_setmetatable(L, "lomemo.Session");
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
  const char *s = luaL_checkstring(L, 1);
  size_t n = lua_rawlen(L, 1);
  struct omemoStore *store = lua_newuserdatauv(L, sizeof(struct omemoStore), 0);
  if (omemoDeserializeStore(s, n, store))
    luaL_error(L, "omemo: deserializing protobuf");
  luaL_setmetatable(L, "lomemo.Store");
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
    lua_seti(L, -2, ++p);
  }
  lua_setfield(L, -2, "prekeys");
  return 1;
}

static int EncryptMessage(lua_State *L) {
  const char *msg = luaL_checkstring(L, 1);
  uint8_t iv[12];
  omemoKeyPayload key;
  size_t msgn = lua_rawlen(L, 1);
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
    lua_pushstring(L, "omemo: encrypt message failed");
    return 2;
  }
}

static int DecryptMessage(lua_State *L) {
  const char *msg = luaL_checkstring(L, 1);
  const char *key = luaL_checkstring(L, 2);
  const char *iv = luaL_checkstring(L, 3);
  size_t msgn = lua_rawlen(L, 1);
  if (lua_rawlen(L, 2) != sizeof(omemoKeyPayload))
    luaL_argerror(L, 2, "omemo: key size");
  if (lua_rawlen(L, 3) != 12)
    luaL_argerror(L, 3, "omemo: iv size");
  char *dec = Alloc(L, msgn);
  int r = omemoDecryptMessage(dec, key, sizeof(omemoKeyPayload), iv, msg, msgn);
  if (!r) {
    lua_pushlstring(L, dec, msgn);
    free(dec);
    return 1;
  } else {
    free(dec);
    lua_pushnil(L);
    lua_pushstring(L, "omemo: encrypt message failed");
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
  {NULL,NULL},
};

static const luaL_Reg sessionmt[] = {
  {"DecryptKey",DecryptKey},
  {"EncryptKey",EncryptKey},
  {"Serialize",SerializeSession},
  {NULL,NULL},
};

static void RegisterMetatable(lua_State *L, const luaL_Reg *reg, const char *name) {
  luaL_newmetatable(L, name);
  luaL_setfuncs(L, reg, 0);
  lua_setfield(L, -1, "__index");
}

int luaopen_lomemo(lua_State *L) {
  luaL_newlib(L, lib);
  RegisterMetatable(L, storemt, "lomemo.Store");
  RegisterMetatable(L, sessionmt, "lomemo.Session");
  return 1;
}
