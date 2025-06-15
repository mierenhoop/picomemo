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
  lua_State *L;
};

int omemoRandom(void *d, size_t n) {
  assert(getrandom(d, n, 0) == n);
  return 0;
}

int omemoLoadMessageKey(struct omemoSession *, struct omemoMessageKey *k) {
  return 1;
}

int omemoStoreMessageKey(struct omemoSession *, const struct omemoMessageKey *k, uint64_t n) {
  return 1;
}

void *Alloc(lua_State *L, size_t n) {
  void *p;
  if (!(p = malloc(n))) luaL_error(L, "Allocation failed");
  return p;
}

static int SetupStore(lua_State *L) {
  struct omemoStore *store = lua_newuserdatauv(L, sizeof(struct omemoStore), 0);
  luaL_setmetatable(L, "omemo.Store");
  if (omemoSetupStore(store))
    luaL_error(L, "setup store");
  return 1;
}

static void CopySizedField(lua_State *L, int i, const char *f, int n, uint8_t *d) {
  lua_getfield(L, i, f);
  const char *s = luaL_checkstring(L, -1);
  if (lua_rawlen(L, -1) == n)
    memcpy(d, s, n);
  else
    luaL_error(L, "field not right size");
}

static int InitFromBundle(lua_State *L) {
  struct omemoStore *store = luaL_checkudata(L, 1, "omemo.Store");
  struct Session *session = lua_newuserdatauv(L, sizeof(struct Session), 0);
  struct omemoBundle bundle = {0};
#define C(f) CopySizedField(L, 2, #f, sizeof(bundle.f), bundle.f)
  C(spks);
  C(spk);
  C(ik);
  C(pk);
  luaL_setmetatable(L, "omemo.Session");
  int r = omemoInitFromBundle((struct omemoSession*)session, store, &bundle);
  if (r) {
    return 1;
  } else {
    lua_pop(L, 1);
    lua_pushnil(L);
    lua_pushstring(L, "error");
    return 2;
  }
}

static int DeserializeStore(lua_State *L) {
  const char *s = luaL_checkstring(L, 1);
  size_t n = lua_rawlen(L, 1);
  struct omemoStore *store = lua_newuserdatauv(L, sizeof(struct omemoStore), 0);
  if (omemoDeserializeStore(s, n, store))
    luaL_error(L, "omemo: deserializing protobuf");
  luaL_setmetatable(L, "omemo.Store");
  return 1;
}

static int SerializeStore(lua_State *L) {
  struct omemoStore *store = luaL_checkudata(L, 1, "omemo.Store");
  size_t n = omemoGetSerializedStoreSize(store);
  uint8_t *p = Alloc(L, n);
  omemoSerializeStore(p, store);
  lua_pushlstring(L, p, n);
  free(p);
  return 1;
}

static int GetBundle(lua_State *L) {
  omemoSerializedKey spk, ik;
  struct omemoStore *store = luaL_checkudata(L, 1, "omemo.Store");
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

static const luaL_Reg lib[] = {
  {"SetupStore", SetupStore},
  {"DeserializeStore", DeserializeStore},
  {"InitFromBundle", InitFromBundle},
  {NULL,NULL},
};

static const luaL_Reg storemt[] = {
  {"Serialize",SerializeStore},
  {"GetBundle",GetBundle},
  {NULL,NULL},
};

static const luaL_Reg sessionmt[] = {
  //{"DecryptKey",DecryptKey},
  {NULL,NULL},
};

static void RegisterMetatable(lua_State *L, const luaL_Reg *reg, const char *name) {
  luaL_newmetatable(L, name);
  luaL_setfuncs(L, reg, 0);
  lua_setfield(L, -1, "__index");
}

int luaopen_lomemo(lua_State *L) {
  luaL_newlib(L, lib);
  RegisterMetatable(L, storemt, "omemo.Store");
  RegisterMetatable(L, sessionmt, "omemo.Session");
  return 1;
}
