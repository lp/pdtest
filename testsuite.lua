mysuite = pdtest.suite("TestSuite")
mysuite.setup( function() pdtest.raw_message({"command","flushdb"}) end)
mysuite.teardown( function() pdtest.raw_message({"command","flushdb"}) end)

mysuite.case("SET String"
  ).test({"command", "SET", "FOO", "BAR"}
    ).should().equal({"OK"})

mysuite.case("GET String"
  ).setup( function() pdtest.raw_message({"command","SET","FOO","BAR"}) end
  ).test({"command", "GET", "FOO"}
    ).should().equal({"BAR"})