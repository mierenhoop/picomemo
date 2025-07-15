-- Query stanza
local function Q(st, name, xmlns)
  if st and type(st) == "table" then
    for _, c in ipairs(st) do
      if type(c) == "table" and c[0] == name and c.xmlns == xmlns then
        return c
      end
    end
  end
end

local function S(s) assert(type(s) == "string", "xml: expected string") return s end

return {Q=Q, S=S}
