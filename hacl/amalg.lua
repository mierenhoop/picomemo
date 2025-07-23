local out = ""

function Add(filename)
  local content = assert(io.open(filename), filename):read("*a")
  out = out .. content:gsub("#include \".-\n",""):gsub("#pragma once","")
end


Add("krml/internal/compat.h")
Add("krml/internal/target.h")
Add("krml/internal/types.h")
Add("krml/lowstar_endianness.h")

Add("internal/Hacl_Krmllib.h")
Add("Hacl_Krmllib.h")

Add("Hacl_IntTypes_Intrinsics.h")
Add("lib_intrinsics.h")

Add("FStar_UInt128.h")
Add("FStar_UInt_8_16_32_64.h")
Add("fstar_uint128_gcc64.h")

Add("internal/Hacl_Streaming_Types.h")
Add("Hacl_Streaming_Types.h")

Add("internal/Hacl_Bignum_Base.h")
Add("internal/Hacl_Bignum25519_51.h")
Add("internal/Hacl_Curve25519_51.h")
Add(         "Hacl_Curve25519_51.h")

Add("internal/Hacl_Hash_SHA2.h")
Add(         "Hacl_Hash_SHA2.h")

Add("internal/Hacl_Ed25519_PrecompTable.h")
Add("internal/Hacl_Ed25519.h")

Add("Hacl_Curve25519_51.c")
Add("Hacl_Hash_SHA2.c")
Add("Hacl_Ed25519.c")

local f = io.open("/tmp/out.c", "w")
f:write(out)
f:close()
os.execute("cc /tmp/out.c")
