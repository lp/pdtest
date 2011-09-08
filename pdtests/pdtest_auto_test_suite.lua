local suite = _.suite("Auto Test")

suite.case("test lists"
  ).test({"A","B","C"}
    ).should:equal({"A","B","C"})
suite.case("not test lists"
  ).test({"A","B","C"}
    ).should.nt:equal({"B","B","C"})
    
suite.case("out lists"
  ).test(function()
    _.test.list({"A","B","C"})
  end).should:equal({"A","B","C"})

suite.case("test symbol"
  ).test("OK"
    ).should:equal("OK")
    
suite.case("out symbol"
  ).test(function()
    _.test.symbol("OK")
  end).should:equal("OK")

suite.case("test float"
  ).test(1.0
    ).should:equal(1.0)

suite.case("out float"
  ).test(function()
    _.test.float(1.0)
  end).should:equal(1.0)

suite.case("test bang"
  ).test("bang"
    ).should:equal("bang")

suite.case("out bang"
  ).test(function()
    _.test.bang()
  end).should:equal("bang")

suite.case("be more as numbers"
  ).test(3
    ).should:be_more(2)

suite.case("not be more as numbers"
  ).test(2
    ).should.nt:be_more(3)

suite.case("be more as string and number"
  ).test("10"
    ).should:be_more(2)

suite.case("not be more as string and number"
  ).test("10"
    ).should.nt:be_more(11)

suite.case("be more as number and string"
  ).test(10
    ).should:be_more("2")

suite.case("not be more as number and string"
  ).test(10
    ).should.nt:be_more("11")

suite.case("be more as table number and number"
  ).test({10,1,2}
    ).should:be_more(5)

suite.case("not be more as table number and number"
  ).test({10,1,2}
    ).should.nt:be_more(11)

suite.case("be more as table number and string"
  ).test({10,1,2}
    ).should:be_more("5")

suite.case("not be more as table number and string"
  ).test({10,1,2}
    ).should.nt:be_more("11")

suite.case("be more as table number and table number"
  ).test({10,1,2}
    ).should:be_more({5,100,200})

suite.case("not be more as table number and table number"
  ).test({10,1,2}
    ).should.nt:be_more({11,0,1})

suite.case("be more as table number and table string"
  ).test({10,1,2}
    ).should:be_more({"5","100","200"})

suite.case("not be more as table number and table string"
  ).test({10,1,2}
    ).should.nt:be_more({"11","0","1"})

suite.case("be more as table string and number"
  ).test({"10","1","2"}
    ).should:be_more(5)

suite.case("not be more as table string and number"
  ).test({"10","1","2"}
    ).should.nt:be_more(11)

suite.case("be more as table string and string"
  ).test({"10","1","2"}
    ).should:be_more("5")

suite.case("not be more as table string and string"
  ).test({"10","1","2"}
    ).should.nt:be_more("11")

suite.case("be more as table string and table number"
  ).test({"10","1","2"}
    ).should:be_more({5,100,200})

suite.case("not be more as table string and table number"
  ).test({"10","1","2"}
    ).should.nt:be_more({11,0,1})

suite.case("be more as table string and table string"
  ).test({"10","1","2"}
    ).should:be_more({"5","100","200"})

suite.case("not be more as table string and table string"
  ).test({"10","1","2"}
    ).should.nt:be_more({"11","0","1"})

suite.case("be less as numbers"
  ).test(3
    ).should:be_less(4)

suite.case("not be less as numbers"
  ).test(4
    ).should.nt:be_less(3)

suite.case("be less as string and number"
  ).test("10"
    ).should:be_less(11)

suite.case("not be less as string and number"
  ).test("10"
    ).should.nt:be_less(9)

suite.case("be less as number and string"
  ).test(10
    ).should:be_less("11")

suite.case("not be less as number and string"
  ).test(10
    ).should.nt:be_less("9")

suite.case("be less as table number and number"
  ).test({10,1,2}
    ).should:be_less(11)

suite.case("not be less as table number and number"
  ).test({10,1,2}
    ).should.nt:be_less(9)

suite.case("be less as table number and string"
  ).test({10,1,2}
    ).should:be_less("11")

suite.case("not be less as table number and string"
  ).test({10,1,2}
    ).should.nt:be_less("9")

suite.case("be less as table number and table number"
  ).test({10,1,2}
    ).should:be_less({11,100,200})

suite.case("not be less as table number and table number"
  ).test({10,1,2}
    ).should.nt:be_less({9,0,1})

suite.case("be less as table number and table string"
  ).test({10,1,2}
    ).should:be_less({"20","100","200"})

suite.case("not be less as table number and table string"
  ).test({10,1,2}
    ).should.nt:be_less({"5","0","1"})

suite.case("be less as table string and number"
  ).test({"10","1","2"}
    ).should:be_less(20)

suite.case("not be less as table string and number"
  ).test({"10","1","2"}
    ).should.nt:be_less(5)

suite.case("be less as table string and string"
  ).test({"10","1","2"}
    ).should:be_less("20")

suite.case("not be less as table string and string"
  ).test({"10","1","2"}
    ).should.nt:be_less("5")

suite.case("be less as table string and table number"
  ).test({"10","1","2"}
    ).should:be_less({20,100,200})

suite.case("not be less as table string and table number"
  ).test({"10","1","2"}
    ).should.nt:be_less({5,0,1})

suite.case("be less as table string and table string"
  ).test({"10","1","2"}
    ).should:be_less({"20","100","200"})

suite.case("not be less as table string and table string"
  ).test({"10","1","2"}
    ).should.nt:be_less({"5","0","1"})

suite.case("be more or equal as numbers"
  ).test(3
    ).should:be_more_or_equal(2)

suite.case("not be more or equal as numbers"
  ).test(2
    ).should.nt:be_more_or_equal(3)

suite.case("be more or equal as string and number"
  ).test("10"
    ).should:be_more_or_equal(10)

suite.case("not be more or equal as string and number"
  ).test("10"
    ).should.nt:be_more_or_equal(11)

suite.case("be more or equal as number and string"
  ).test(10
    ).should:be_more_or_equal("10")

suite.case("not be more or equal as number and string"
  ).test(10
    ).should.nt:be_more_or_equal("11")

suite.case("be more or equal as table number and number"
  ).test({10,1,2}
    ).should:be_more_or_equal(5)

suite.case("not be more or equal as table number and number"
  ).test({10,1,2}
    ).should.nt:be_more_or_equal(11)

suite.case("be more or equal as table number and string"
  ).test({10,1,2}
    ).should:be_more_or_equal("10")

suite.case("not be more or equal as table number and string"
  ).test({10,1,2}
    ).should.nt:be_more_or_equal("11")

suite.case("be more or equal as table number and table number"
  ).test({10,1,2}
    ).should:be_more_or_equal({5,100,200})

suite.case("not be more or equal as table number and table number"
  ).test({10,1,2}
    ).should.nt:be_more_or_equal({11,0,1})

suite.case("be more or equal as table number and table string"
  ).test({10,1,2}
    ).should:be_more_or_equal({"10","100","200"})

suite.case("not be more or equal as table number and table string"
  ).test({10,1,2}
    ).should.nt:be_more_or_equal({"11","0","1"})

suite.case("be more or equal as table string and number"
  ).test({"10","1","2"}
    ).should:be_more_or_equal(5)

suite.case("not be more or equal as table string and number"
  ).test({"10","1","2"}
    ).should.nt:be_more_or_equal(11)

suite.case("be more or equal as table string and string"
  ).test({"10","1","2"}
    ).should:be_more_or_equal("10")

suite.case("not be more or equal as table string and string"
  ).test({"10","1","2"}
    ).should.nt:be_more_or_equal("11")

suite.case("be more or equal as table string and table number"
  ).test({"10","1","2"}
    ).should:be_more_or_equal({5,100,200})

suite.case("not be more or equal as table string and table number"
  ).test({"10","1","2"}
    ).should.nt:be_more_or_equal({11,0,1})

suite.case("be more or equal as table string and table string"
  ).test({"10","1","2"}
    ).should:be_more_or_equal({"10","100","200"})

suite.case("not be more or equal as table string and table string"
  ).test({"10","1","2"}
    ).should.nt:be_more_or_equal({"11","0","1"})

suite.case("be less or equal as numbers"
  ).test(3
    ).should:be_less_or_equal(4)

suite.case("not be less or equal as numbers"
  ).test(4
    ).should.nt:be_less_or_equal(3)

suite.case("be less or equal as string and number"
  ).test("10"
    ).should:be_less_or_equal(10)

suite.case("not be less or equal as string and number"
  ).test("10"
    ).should.nt:be_less_or_equal(9)

suite.case("be less or equal as number and string"
  ).test(10
    ).should:be_less_or_equal("11")

suite.case("not be less or equal as number and string"
  ).test(10
    ).should.nt:be_less_or_equal("9")

suite.case("be less or equal as table number and number"
  ).test({10,1,2}
    ).should:be_less_or_equal(10)

suite.case("not be less or equal as table number and number"
  ).test({10,1,2}
    ).should.nt:be_less_or_equal(9)

suite.case("be less or equal as table number and string"
  ).test({10,1,2}
    ).should:be_less_or_equal("11")

suite.case("not be less or equal as table number and string"
  ).test({10,1,2}
    ).should.nt:be_less_or_equal("9")

suite.case("be less or equal as table number and table number"
  ).test({10,1,2}
    ).should:be_less_or_equal({10,100,200})

suite.case("not be less or equal as table number and table number"
  ).test({10,1,2}
    ).should.nt:be_less_or_equal({9,0,1})

suite.case("be less or equal as table number and table string"
  ).test({10,1,2}
    ).should:be_less_or_equal({"20","100","200"})

suite.case("not be less or equal as table number and table string"
  ).test({10,1,2}
    ).should.nt:be_less_or_equal({"5","0","1"})

suite.case("be less or equal as table string and number"
  ).test({"10","1","2"}
    ).should:be_less_or_equal(10)

suite.case("not be less or equal as table string and number"
  ).test({"10","1","2"}
    ).should.nt:be_less_or_equal(5)

suite.case("be less or equal as table string and string"
  ).test({"10","1","2"}
    ).should:be_less_or_equal("20")

suite.case("not be less or equal as table string and string"
  ).test({"10","1","2"}
    ).should.nt:be_less_or_equal("5")

suite.case("be less or equal as table string and table number"
  ).test({"10","1","2"}
    ).should:be_less_or_equal({10,100,200})

suite.case("not be less or equal as table string and table number"
  ).test({"10","1","2"}
    ).should.nt:be_less_or_equal({5,0,1})

suite.case("be less or equal as table string and table string"
  ).test({"10","1","2"}
    ).should:be_less_or_equal({"20","100","200"})

suite.case("not be less or equal as table string and table string"
  ).test({"10","1","2"}
    ).should.nt:be_less_or_equal({"5","0","1"})

suite.case("match list"
  ).test({"A","B","C","D"}
    ).should:match("^%u$")

suite.case("not match list"
  ).test({"A","B","C","D"}
    ).should.nt:match("^%d$")

suite.case("match string"
  ).test("1 special string"
    ).should:match("^%d%s%a+%s%a+$")

suite.case("not match string"
  ).test("1 special string"
    ).should.nt:match("^%w+$")

suite.case("match number"
  ).test(286
    ).should:match("^%d%d%d$")

suite.case("not match number"
  ).test(2990
    ).should.nt:match("^%d%d%d$")
    
suite.case("resemble list"
  ).test({"A","B","C","D"}
    ).should:resemble({"B","C","D","A"})

suite.case("not resemble list"
  ).test({"A","B","B","D"}
    ).should.nt:resemble({"B","C","D","A"})

suite.case("resemble string"
  ).test("okeedookie"
    ).should:resemble("oKeeDooKIe")

suite.case("not resemble string"
  ).test("okeedooki"
    ).should.nt:resemble("OkEEdOOkie")

suite.case("string resemble number"
  ).test("26"
    ).should:resemble(26)

suite.case("string not resemble number"
  ).test("26"
    ).should.nt:resemble(26.66)

suite.case("number resemble string"
  ).test(26
    ).should:resemble("26")

suite.case("number not resemble string"
  ).test(26
    ).should.nt:resemble("26.6")

suite.case("resemble number"
  ).test(26.66667
    ).should:resemble(26.66,1)

suite.case("not resemble number"
  ).test(26
    ).should.nt:resemble(26.66)

suite.case("list resemble string"
  ).test({"A","B","C","D"}
    ).should:resemble("a")

suite.case("list not resemble string"
  ).test({"A","B","C","D"}
    ).should.nt:resemble("B")

suite.case("string resemble list"
  ).test("A"
    ).should:resemble({"a","b","c"})
    
suite.case("string not resemble list"
  ).test("B"
    ).should.nt:resemble({"a","b","c"})

suite.case("list resemble number"
  ).test({"2.666","3.3333","4.8989"}
    ).should:resemble(2.66,1)

suite.case("list not resemble number"
  ).test({"2.666","3.3333","4.8989"}
    ).should.nt:resemble(3.33,1)

suite.case("number resemble list"
  ).test(3.333
    ).should:resemble({3.33,2.66,1.8787},1)
    
suite.case("number not resemble list"
  ).test(3.333
    ).should.nt:resemble({3.33,2.66,1.8787})

suite.case("be_any number"
  ).test(4
    ).should:be_any({1,2,3,4,5,6,7,8,9})

suite.case("not be_any number"
  ).test(4
    ).should.nt:be_any({1,2,3,5,6,7,8,9})
    
suite.case("be_any string"
  ).test("bozo"
    ).should:be_any({"ronald","dingo","bozo"})
    
suite.case("not be_any string"
  ).test("bozo"
    ).should.nt:be_any({"ronald","dingo","marcel"})
    
suite.case("be_any table"
  ).test({"A","B","C","D"}
    ).should:be_any({{"E","F","G"},{"A","B","C","D"},{"X","Y","Z"}})
