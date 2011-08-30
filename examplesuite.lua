mysuite = pdtest.suite("TestSuite")     -- initializing a new test suite named "TestSuite"

mysuite.setup(function()                -- called before every test in the suite
  pdtest.raw_message({"command","flushdb"})
end)

mysuite.teardown(function()             -- called after every test in the suite
  pdtest.raw_message({"command","flushdb"})
end)

mysuite.case("Server Info"              -- new test case named "Server Info"
  ).test({"command", "INFO"}            -- will output message "list command INFO"
    ).should:match("^redis_version")    -- test will pass if result received on right 
                                        -- inlet starts with the string "redis_version"

mysuite.case("Reality Check"            -- second test case named "Reality Check"
  ).test({"command", "dummy"}           -- will output message "list command dummy"
    ).should:match("ERR"                -- test will pass if result received on right
  ).test({"command", "dbsize"}          -- inlet match the string "ERR"
    ).should.nt:match("ERR")            -- second test will pass if result doesn't match "ERR"

mycase = mysuite.case("Basic tests")    -- new test case named "Basic tests"

mycase.setup(                           -- called before every test in the case
  function() pdtest.raw_message({"command","SET","FOO","BAR"})
end)

mycase.test({"command", "GET", "FOO"}   -- test will pass if result equals "BAR"
  ).should:equal({"BAR"})

mycase.test(function()                  -- here a test function is passed instead
  pdtest.raw_message({"command","DEL","FOO"})
  pdtest.message({"command","EXISTS","FOO"})
end).should:equal("0")                  -- equal is passed a string, when result list
                                        -- contains only one member

