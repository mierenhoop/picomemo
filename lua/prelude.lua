-- error when assigning or indexing global
setmetatable(_G, {
  __index = function(self,k) error("trying to index global '"..k.."'") end,
  __newindex = function(self,k) error("trying to assign global '"..k.."'") end,
})
