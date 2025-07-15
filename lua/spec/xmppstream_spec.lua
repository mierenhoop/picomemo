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
  it("should allow omitting xml declaration",
    TestInputs({"<stream:stream  xml:lang='en' version=\"1.0\" >"},
    {{[0]="stream:stream",["xml:lang"]="en",version="1.0"}}))
  it("should parse CDATA",
    TestInputs({"<stream:stream><x>asdf<![CDATA[<><f.dakfjk'''\"'>>]> ]]e]]]o]>]]>fjkdjafk</x>"},
    {{[0]="stream:stream"},{[0]="x","asdf<><f.dakfjk'''\"'>>]> ]]e]]]o]>fjkdjafk"}}))
  it("should parse empty CDATA",
    TestInputs({"<stream:stream><x><![CDATA[]]></x>"},
    {{[0]="stream:stream"},{[0]="x",""}}))
  it("should parse fragmented CDATA",
    TestInputs({"<stream:stream><x><![CDATA".."[".."e".."]".."]".."></x>"},
    {{[0]="stream:stream"},{[0]="x","e"}}))
end)
