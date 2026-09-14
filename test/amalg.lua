local MARKER = "// OMEMO Additions"
local USAGE = "usage: lua amalg.lua hacl|c25519 SRCDIR OUT"

local target, srcdir, outpath = ...
assert(srcdir and outpath, USAGE)

local function Read(filename)
  local f = assert(io.open(filename))
  local s = f:read("*a")
  f:close()
  return s
end

local function Write(filename, s)
  local f = assert(io.open(filename, "w"))
  f:write(s)
  f:close()
end

local function Tail(filename)
  local s = Read(filename)
  local i = assert(s:find(MARKER, 1, true), "marker not found in " .. filename)
  return s:sub(i)
end

-- gsub exactly once
local function Sub(s, pattern, repl)
  local r, n = s:gsub(pattern, repl)
  assert(n == 1, pattern .. " matched " .. n .. " times")
  return r
end

local function Hacl()
  local function ReadSource(filename)
    for _, dir in ipairs{
      "/dist/gcc-compatible/",
      "/dist/karamel/include/",
      "/dist/karamel/krmllib/dist/minimal/",
    } do
      local f = io.open(srcdir .. dir .. filename)
      if f then
        local s = f:read("*a")
        f:close()
        return s
      end
    end
    error(filename .. " not found in " .. srcdir)
  end

  local out = {[[
// Amalgamation of a stripped down hacl*. Generated with
// `make amalg-hacl`.
]]}

  local function Add(filename)
    out[#out+1] = (ReadSource(filename):gsub("#include \".-\n",""):gsub("#pragma once",""))
  end

  out[#out+1] = [[
#ifdef __x86_64__
#define HACL_CAN_COMPILE_INTRINSICS 1
#endif
#define HACL_CAN_COMPILE_UINT128 1

#define KRML_HOST_PRINTF(...) (void)0
#define KRML_HOST_EPRINTF(...) (void)0
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

  out[#out+1] = "#ifndef HACL_CAN_COMPILE_INTRINSICS\n"
  Add("Hacl_IntTypes_Intrinsics.h")
  Add("Hacl_IntTypes_Intrinsics_128.h")
  out[#out+1] = "#endif\n"

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

  return table.concat(out)
end

local function C25519()
  local out = {[[
// Amalgamation of a stripped down c25519 by Daniel Beer. Generated with
// `make amalg-c25519`.
]]}
  for _, name in ipairs{ "f25519.h", "c25519.h", "ed25519.h",
    "edsign.h", "fprime.h", "morph25519.h", "sha512.h",
    "c25519.c", "ed25519.c", "edsign.c", "f25519.c", "fprime.c",
    "morph25519.c", "sha512.c" } do
    out[#out+1] = Read(srcdir .. "/src/" .. name):gsub("#include \"[^\n]*\"", "")
  end
  local s = table.concat(out)
  s = s:gsub("void edsign_sec_to_pub%(uint8_t %*pub, ",
             "void edsign_sec_to_pub(uint8_t *pub, uint8_t *prv, ")
  s = Sub(s, "static void mx2ey%(", "void morph25519_mx2ey(")
  s = Sub(s, "\tmx2ey%(", "\tmorph25519_mx2ey(")
  s = Sub(s, "(void edsign_sec_to_pub%([^\n]+\n{.-)}",
             "%1\tmemcpy(prv, expanded, 32);\n}");
  return s.."\n"
end

local generate = assert(({hacl = Hacl, c25519 = C25519})[target], USAGE)

Write(outpath, generate() .. Tail(outpath))
