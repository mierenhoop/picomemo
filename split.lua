for _,ext in ipairs{".c",".h"} do
  for _,version in ipairs{"omemo","omemo2"} do
    local s = io.open("omemo"..ext):read"*a"
    s = s:gsub("#if(n?)def OMEMO2(.-)#endif", function(n,body)
      local a, b = body:match"(.-)#else(.+)"
      if not a then a, b = body, "" end
      return (n == "") == (version == "omemo2") and a or b
    end)
    s=s:gsub("OMEMO_",version:upper().."_")
    s=s:gsub("omemo(%u)",version.."%1")
    io.open("/tmp/"..version..ext,"w"):write(s)
  end
end
