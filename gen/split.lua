for _,ext in ipairs{".c",".h"} do
  for _,version in ipairs{"omemo0","omemo2"} do
    local s = io.open("omemo"..ext):read"*a"
    s = s:gsub("#if(n?)def OMEMO2\n(.-)#endif\n", function(n,body)
      local a, b = body:match"(.-)#else(.+)"
      if not a then a, b = body, "" end
      return (n == "") == (version == "omemo2") and a or b
    end)
    s=s:gsub("omemo%.h",version..".h")
    s=s:gsub("OMEMO_",version:upper().."_")
    s=s:gsub("omemo(%u)",version.."%1")
    -- Don't change omemoDriver
    s=s:gsub(version.."Driver","omemoDriver")
    io.open("gen/"..version..ext,"w"):write(s)
  end
end
