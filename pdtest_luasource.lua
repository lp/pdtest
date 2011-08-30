-- from C
pdtest = {}

-- pdtest_init
pdtest.suite = function(suite)
  currentSuite = {name=suite, queue={}, dones={}}
  
  currentSuite.before = function() end
  currentSuite.after = function() end
  currentSuite.setup = function(setup)
    if type(setup) == "function" then
      currentSuite.before = setup
      return currentSuite
    end
  end
  currentSuite.teardown = function(teardown)
    if type(teardown) == "function" then
      currentSuite.after = teardown
      return currentSuite
    end
  end
  
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
      currentTest = {test=test, suite=currentSuite, case=currentCase}
      
      cmpmet = {}
      cmpmet.report = function(self,okmsg,failmsg,success,should,result)
        if self.invert then
          if success then
            return false, ""..table.concat(should, ", ")..okmsg..table.concat(result,", ")..""
          else
            return true, ""..table.concat(should, ", ")..failmsg..table.concat(result,", ")..""
          end
        else
          if success then
            return true, ""..table.concat(should, ", ")..okmsg..table.concat(result,", ")..""
          else
            return false, ""..table.concat(should, ", ")..failmsg..table.concat(result,", ")..""
          end
        end
      end
      
      cmpmet.equal = function(self,should)
        currentTest.try = function(result)
          if type(should) == "table" and type(result) == "table" then
            same = true
            for i,v in ipairs(should) do
              if v ~= result[i] then
                same = false
              end
            end
            return self:report(" is equal to "," is not equal to ",same,should,result)
          else
            return false, "Comparison data needs to be tables: should is '"..type(should).."', result is '"..type(result).."'"
          end
        end
        return currentTest
      end
      
      currentTest.should = {invert=false}
      currentTest.should.nt = {invert=true}
      for k,v in pairs(cmpmet) do
        currentTest.should[k] = v
        currentTest.should.nt[k] = v
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
  if table.getn(pdtest.queue) == 0 then
    return false
  elseif table.getn(pdtest.queue[1].queue) == 0 then
    table.insert(pdtest.dones, table.remove(pdtest.queue,1))
  elseif table.getn(pdtest.queue[1].queue[1].queue) == 0 then
    table.insert(pdtest.queue[1].dones, table.remove(pdtest.queue[1].queue,1))
  else
    current = pdtest.queue[1].queue[1].queue[1]
    current.suite.before()
    current.case.before()
    if type(current.test) == "function" then
      current.test()
    elseif type(current.test) == "table" then
      pdtest.message(current.test)
    else
      pdtest.error("wrong test data type -- "..type(current.test).." -- should have been function or table")
    end
    current.suite.after()
    current.case.after()
    table.insert(pdtest.currents, current)
    table.insert(pdtest.queue[1].queue[1].dones, table.remove(pdtest.queue[1].queue[1].queue,1))
  end
  return true
end

function pdtest_yield()
  if table.getn(pdtest.currents) > 0 and table.getn(pdtest.results) > 0 then
    current = table.remove(pdtest.currents,1)
    result = table.remove(pdtest.results,1)
    current.result = result
    current.success, current.detail = current.try(current.result)
    nametag = ""..current.suite.name.." -> "..current.case.name
    if current.success then
      pdtest.post(nametag.." |> OK")
    else
      pdtest.post(nametag.." |> FAILED >"..current.detail)
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
