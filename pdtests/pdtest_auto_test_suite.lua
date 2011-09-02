suite = pdtest.suite("Auto Test")

suite.case("test lists"
  ).test({"A","B","C"}
    ).should:equal({"A","B","C"})
    
suite.case("out lists"
  ).test(function()
    pdtest.out.list({"A","B","C"})
  end).should:equal({"A","B","C"})

suite.case("test symbol"
  ).test("OK"
    ).should:equal("OK")
    
suite.case("out symbol"
  ).test(function()
    pdtest.out.symbol("OK")
  end).should:equal("OK")

suite.case("test float"
  ).test(1.0
    ).should:equal(1.0)

suite.case("out float"
  ).test(function()
    pdtest.out.float(1.0)
  end).should:equal(1.0)

suite.case("test bang"
  ).test("bang"
    ).should:equal("bang")

suite.case("out bang"
  ).test(function()
    pdtest.out.bang()
  end).should:equal("bang")
