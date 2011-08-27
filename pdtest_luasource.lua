-- from C
pdtest = {}

-- pdtest_init
pdtest.queue={}
pdtest.dones={}
pdtest.results={}
pdtest.currents={}
pdtest.try = 0;
pdtest.suite = function(suite)
  currentSuite = {name=suite, queue={}, dones={}}
  currentSuite.setup = function() end
  currentSuite.teardown = function() end
  
  currentSuite.case = function(case)
    currentCase = {name=case, queue={}, dones={}}
    currentCase.setup = function() end
    currentCase.teardown = function() end
    
    currentCase.test = function(test)
      currentTest = {test=test}
      
      currentTest.should = function(should)
        currentTest.should = should
        
        return currentCase
      end
      
      table.insert(currentCase.queue,currentTest)
      return currentTest
    end
    
    table.insert(currentSuite.queue,currentCase)
    return currentCase
  end
  
  table.insert(pdtest.queue,currentSuite)
  return currentSuite
end

function pdtest_next()
  pdtest.post("*** lua next")
  if table.getn(pdtest.queue) == 0 then
    pdtest.post("*** lua next no suites")
    return false
  elseif table.getn(pdtest.queue[1].queue) == 0 then
    pdtest.post("*** lua next no cases")
    table.insert(pdtest.dones, table.remove(pdtest.queue,1))
  elseif table.getn(pdtest.queue[1].queue[1].queue) == 0 then
    pdtest.post("*** lua next no tests")
    table.insert(pdtest.queue[1].dones, table.remove(pdtest.queue[1].queue,1))
  else
    pdtest.post("*** lua next test")
    current = pdtest.queue[1].queue[1].queue[1]
    if type(current.test) == "function" then
      pdtest.post("*** lua next test function")
      current.test()
    elseif type(current.test) == "table" then
      pdtest.post("*** lua next test message")
      pdtest.message(current.test)
    else
      pdtest.error("wrong test data type -- "..type(current.test).." -- should have been function or table")
    end
    table.insert(pdtest.currents, current)
    table.insert(pdtest.queue[1].queue[1].dones, table.remove(pdtest.queue[1].queue[1].queue,1))
  end
  return true
end


function pdtest_result(result)
  pdtest.post("lua result")
  if type(result) == "table" then
    pdtest.post("lua result table")
    table.insert(pdtest.results,result)
  else
    pdtest.error("wrong result type -- "..type(result).." -- should have been table")
  end
  return true
end

function pdtest_yield()
  if table.getn(pdtest.currents) > 0 and table.getn(pdtest.results) > 0 then
    pdtest.post("lua yield")
    current = table.remove(pdtest.currents,1)
    result = table.remove(pdtest.results,1)
    current.result = result
    if type(current.should) == "function" then
      current.success = current.should(current.result)
      if current.success then
        pdtest.post(": OK")
      else
        pdtest.post(": FAILED")
      end
    else
      pdtest.error("wrong should() data type -- "..type(current.should).." -- should have been function")
    end
    return true
  elseif table.getn(pdtest.currents) == 0 and table.getn(pdtest.results) == 0 and table.getn(pdtest.queue) == 0 then
    return false
  end
end


function pdtest_report()
  if table.getn(pdtest.currents) > 0 then
    return false
  else
    pdtest.post("!!! Test Completed !!!")
    tests = 0
    cases = 0
    suites = 0
    ok = 0
    fail = 0
    for si, suite in ipairs(pdtest.dones) do
      suites = suites + 1
      for ci, case in ipairs(suite.dones) do
        cases = cases + 1
        for ti, test in ipairs(case.dones) do
          tests = tests + 1
          if test.success then
            ok = ok + 1
          else
            fail = fail + 1
          end
        end
      end
    end
    pdtest.post(""..suites.." Suites | "..cases.." Cases | "..tests.." Tests")
    pdtest.post(""..ok.." tests passed")
    pdtest.post(""..fail.." tests failed")
  end
  return true
end

