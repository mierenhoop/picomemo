local out = ""

function Add(filename)
  local content = assert(io.open(filename), filename):read("*a")
  out = out .. content:gsub("#include \".-\n",""):gsub("#pragma once","")
end

-- includes guarded by ifdefs:
-- lib_intrinsics.h: IntType_...h & ''128.h
-- krml/include/types: krml/lowstar_end msvc, gcc64, Verified, struct_e

out=out..[[
#ifndef __EMSCRIPTEN__
#define HACL_CAN_COMPILE_INTRINSICS 1
#endif
#define HACL_CAN_COMPILE_UINT128 1
]]


Add("krml/internal/compat.h")
Add("krml/internal/target.h")
Add("krml/internal/types.h")
Add("krml/lowstar_endianness.h")

Add("FStar_UInt128.h")
Add("LowStar_Endianness.h");
Add("FStar_UInt_8_16_32_64.h")

Add("fstar_uint128_gcc64.h")

Add("internal/Hacl_Krmllib.h")
Add("Hacl_Krmllib.h")

out=out.."#ifndef HACL_CAN_COMPILE_INTRINSICS\n"
Add("Hacl_IntTypes_Intrinsics.h")
Add("Hacl_IntTypes_Intrinsics_128.h")
out=out.."#endif\n"

Add("lib_intrinsics.h")

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
--
--function Read(filename)
--  --print(filename)
--  return assert(io.open(filename), filename):read"*a"
--end
--local done = {}
--local deps = {}
--function Replace(filename)
--  done[filename] = true
--  local s= Read(filename)
--  return (s:gsub("#include \"(.-)\"", function(inc)
--    --if filename == "krml/internal/types.h" and (inc == "fstar_uint128_msvc.h" or inc == "fstar_uint128_gcc64.h" or inc == "FStar_UInt128_Verified.h") then
--    --  return ""
--    --end
--    inc=inc:gsub("^%.%./","")
--    deps[filename] = deps[filename] or {}
--    table.insert(deps[filename], inc)
--    if not done[inc] then
--      local r = Replace(inc)
--      --if done[inc] then return "" end
--      return r
--    end
--    return ""
--  end))
--end
--Replace("Hacl_Curve25519_51.c")
--print(require"inspect"(deps))
local f = io.open("../hacl.c", "w")
f:write(out)
f:close()
