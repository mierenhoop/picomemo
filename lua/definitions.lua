lomemo = {}

--- @class lomemo.Store: userdata
lomemo.Store = nil

--- @return string binary representation of Store
function lomemo.Store:Serialize() end

--- @return { spks: string, spk: string, spk_id: integer, ik: string, prekeys: { pk: string, id: integer }[] }
function lomemo.Store:GetBundle() end

--- @class lomemo.Session: userdata
lomemo.Session = nil

--- @param store lomemo.Store
--- @param isprekey boolean
--- @param key string encrypted key
--- @param cbs { load: fun(self: lomemo.Session, dh: string, nr: integer): (string|nil), store: fun(self: lomemo.Session, dh: string, nr: integer, mk: string, n: integer): (boolean|nil) } callbacks for storing and loading skipped message keys
--- @return string decrypted key
--- @overload fun(store: lomemo.Store, isprekey: boolean, key: string, cbs { load: fun(self: lomemo.Session, dh: string, nr: integer): (string|nil), store: fun(self: lomemo.Session, dh: string, nr: integer, mk: string, n: integer): (boolean|nil) }): nil, error: string
function lomemo.Session:DecryptKey(store, isprekey, key, cbs) end

--- @param store lomemo.Store
--- @param key string plain key
--- @return string encrypted key, boolean isprekey
--- @overload fun(store: lomemo.Store, key: string): nil, error: string
function lomemo.Session:EncryptKey(store, key) end

--- @return string binary representation of Session
function lomemo.Session:Serialize() end

--- @return lomemo.Store
function lomemo.SetupStore() end

--- @param s string serialized store
--- @return lomemo.Store
function lomemo.DeserializeStore(s) end

--- @param s string serialized session
--- @return lomemo.Session
function lomemo.DeserializeSession(s) end

--- @return lomemo.Session
function lomemo.NewSession() end

--- @param store lomemo.Store
--- @param bundle { spks: string, spk: string, spk_id: integer, ik: string, pk: string, pk_id: integer }
--- @return lomemo.Session
--- @overload fun(store: lomemo.Store, bundle: { spks: string, spk: string, spk_id: integer, ik: string, pk: string, pk_id: integer }): nil, error: string
function lomemo.InitFromBundle(store, bundle) end

--- @param msg string
--- @return string encrypted msg, string key, string iv
--- @overload fun(msg: string): nil, error: string
function lomemo.EncryptMessage(msg) end

--- @param msg string encrypted message
--- @param key string
--- @param iv string
--- @return string decrypted message
--- @overload fun(msg: string, key: string, iv: string): nil, error: string
function lomemo.DecryptMessage(msg, key, iv) end
