local xmppstream = require"xmppstream"

local function TestInputs(inputs, sts)
  return function()
    local i = 0
    local stream = xmppstream()
    for _, inp in ipairs(inputs) do
      local x = stream(inp)
      while x do
        i=i+1
        assert.are.same(sts[i], x)
        x = stream()
      end
    end
    assert.are.same(i, #sts)
  end
end

describe("xmppstream", function()
  it("should not fail on empty input", TestInputs({"",""},{}))
  it("should allow xml declaration",
    TestInputs({"<?xm","l",""," version=\"1.0\" encoding='uTf-8'  standalone='yes'  ?>"},{}))
  it("should allow omitting xml declaration",
    TestInputs({"<stream:stream  xml:lang='en' version=\"1.0\" >"},
    {{[0]="stream:stream",["xml:lang"]="en",version="1.0"}}))
end)
