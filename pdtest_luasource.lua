-- from C
pdtest = {}

-- pdtest_init
pdtest.suite = function(suite)
  currentSuite = {name=suite, queue={}, dones={}}
  
  currentSuite.case = function(case)
    currentCase = {name=case, queue={}, dones={}}
    
    currentCase.before = function() end
    currentCase.after = function() end
    currentCase.setup = function(setup)
      if type(setup) == "function" then
        currentCase.before = setup
        return currentCase
      end
    end
    currentCase.teardown = function(teardown)
      if type(teardown) == "function" then
        currentCase.after = teardown
        return currentCase
      end
    end
    
    currentCase.test = function(test)
      currentTest = {test=test, case=currentCase}
      
      currentTest.should = function()
        cmpmet = {}
        cmpmet.equal = function(should)
          currentTest.try = function(result)
            if type(should) == "table" and type(result) == "table" then
              same = true
              for i,v in ipairs(should) do
                if v ~= result[i] then
                  same = false
                end
              end
              if same then
                return true, ""..table.concat(should, ", ").." is equal to "..table.concat(result,", ")..""
              else
                return false, ""..table.concat(should, ", ").." is not equal to "..table.concat(result,", ")..""
              end
            else
              return false, "Comparison data needs to be tables: should is '"..type(should).."', result is '"..type(result).."'"
            end
          end
          return currentTest
        end
        
        return cmpmet
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
    current.case.before()
    if type(current.test) == "function" then
      pdtest.post("*** lua next test function")
      current.test()
    elseif type(current.test) == "table" then
      pdtest.post("*** lua next test message")
      pdtest.message(current.test)
    else
      pdtest.error("wrong test data type -- "..type(current.test).." -- should have been function or table")
    end
    current.case.after()
    table.insert(pdtest.currents, current)
    table.insert(pdtest.queue[1].queue[1].dones, table.remove(pdtest.queue[1].queue[1].queue,1))
  end
  return true
end

function pdtest_yield()
  pdtest.post("lua yield")
  if table.getn(pdtest.currents) > 0 and table.getn(pdtest.results) > 0 then
    current = table.remove(pdtest.currents,1)
    result = table.remove(pdtest.results,1)
    current.result = result
    current.success, current.detail = current.try(current.result)
    if current.success then
      pdtest.post("-OK")
    else
      pdtest.post("-FAILED: "..current.detail)
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

