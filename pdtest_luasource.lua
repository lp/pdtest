-- from C
pdtest = {}

-- pdtest_init
pdtest.suite = function(suite)
  local currentSuite = {name=suite, queue={}, dones={}}
  
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
    local currentCase = {name=case, queue={}, dones={}}
    
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
      local currentTest = {test=test, suite=currentSuite, case=currentCase}
      
      local cmpmet = {}
      cmpmet.report = function(self,okmsg,failmsg,success,should,result)
        if type(should) == "table" then
          should = table.concat(should, ", ")
        elseif type(should) == "number" then
          should = tostring(should)
        elseif type(should) == "function" then
          should = "function"
        end
        
        if type(result) == "table" then
          result = table.concat(result,", ")
        elseif type(result) == "number" then
          result = tostring(result)
        end
        
        if self.invert then
          if success then
            return false, ""..result..okmsg..should..""
          else
            return true, ""..result..failmsg..should..""
          end
        else
          if success then
            return true, ""..result..okmsg..should..""
          else
            return false, ""..result..failmsg..should..""
          end
        end
      end
      
      cmpmet.equal = function(self,should)
        currentTest.try = function(result)
          local same = true
          if type(should) == "table" and type(result) == "table" then
            for i,v in ipairs(should) do
              if v ~= result[i] then
                same = false
              end
            end
          elseif (type(should) == "string" and type(result) == "string") or
            type(should) == "number" and type(result) == "number" then
            same = should == result
          else
            return false, "Comparison data needs to be of similar type: should is '"..type(should).."', result is '"..type(result).."'"
          end
          return self:report(" is equal to "," is not equal to ",same,should,result)
        end
        return currentCase
      end
      
      cmpmet.resemble = function(self,should)
        currentTest.try = function(result)
          local same = true
          if type(should) == "table" and type(result) == "table" then
            should_set = set.new(should)
            result_set = set.new(result)
            same = set.equal(should_set,result_set)
          elseif type(should) == type(result) then
            same = should == result
          elseif (type(should) == "string" and type(result) == "number") or
            (type(result) == "string" and type(should) == "number") then
            same = tostring(should) == tostring(result)
          else
            if type(should) == "table" then
              same = tostring(should[1]) == tostring(result)
            elseif type(result) == "table" then
              same = tostring(result[1]) == tostring(should)
            end
          end
          return self:report(" does resemble "," does not resemble ",same,should,result)
        end
      end
      
      cmpmet.be_nil = function(self)
        currentTest.try = function(result)
          local same = true
          if type(result) == "table" then
            for i,v in ipairs(result) do
              if result[i] ~= nil then
                same = false
              end
            end
          elseif type(result) == "string" or type(result) == "number" then
            same = false
          elseif type(result) == nil then
            same = true
          else
            return false, "Comparison result needs to be table, string or number: result is '"..type(result).."'"
          end
          
          return self:report(" is nil "," is not nil ",same,"nil",result)
        end
        return currentCase
      end
      
      cmpmet.match = function(self,match)
        currentTest.try = function(result)
          if type(match) == "string" then
            local same = false
            if type(result) == "table" then
              for i,v in ipairs(result) do
                if string.find(result[i],match) ~= nil then
                  same = true
                end
              end
            elseif type(result) == "string" then
              if string.find(result,match) ~= nil then
                same = true
              end
            elseif type(result) == "number" then
              if string.find(tostring(result),match) ~= nil then
                same = true
              end
            else
              return false, "Comparison result needs to be list,string or number: result is '"..type(result).."'"
            end
            
            return self:report(" does match "," does not match ",same,match,result)
          else
            return false, "Comparison data needs to be string and table: should is '"..type(should).."', result is '"..type(result).."'"
          end
        end
        return currentCase
      end
      
      cmpmet.be_true = function(self,should)
        currentTest.try = function(result)
          if type(should) == "function" then
            local same = should(result)
            return self:report(" is true "," is not true ",same,should,result)
          else
            return false, "Comparison data needs to be function: should is '"..type(should).."'"
          end
        end
        return currentCase
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
    local current = pdtest.queue[1].queue[1].queue[1]
    current.suite.before()
    current.case.before()
    if type(current.test) == "function" then
      current.name = "function"
      current.test()
    elseif type(current.test) == "table" then
      current.name = table.concat(current.test, ", ")
      pdtest.out.list(current.test)
    elseif type(current.test) == "string" then
      current.name = current.test
      pdtest.out.symbol(current.test)
    elseif type(current.test) == "number" then
      current.name = tostring(current.test)
      pdtest.out.float(current.test)
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
    local current = table.remove(pdtest.currents,1)
    local result = table.remove(pdtest.results,1)
    current.result = result
    current.success, current.detail = current.try(current.result)
    local nametag = ""..current.suite.name.." -> "..current.case.name.." < "..current.name.." > "
    if current.success then
      pdtest.post(nametag)
      pdtest.post("-> OK")
    else
      pdtest.post(nametag)
      pdtest.post("x> FAILED |> "..current.detail)
    end
  elseif table.getn(pdtest.currents) == 0 and table.getn(pdtest.results) == 0 and table.getn(pdtest.queue) == 0 then
    return false
  end
  return true
end

function pdtest_report()
  if table.getn(pdtest.currents) > 0 then
    return false
  else
    pdtest.post("!!! Test Completed !!!")
    local tests = 0
    local cases = 0
    local suites = 0
    local ok = 0
    local fail = 0
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
