-- from C
_ = {}

-- pdtest_init
Suite = function(suite)
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
          if type(should[1]) == "table" then
            local nshould = {}
            for i,v in ipairs(should) do
              if type(v) == "table" then
                nshould[i] = table.concat(v,", ")
              else
                nshould[i] = "nil"
              end
            end
            should = table.concat(nshould, " | ")
          else
            should = table.concat(should, ", ")
          end
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
      
      cmpmet.numbers = function(self,should,result)
        if type(should) == "number" and type(result) == "number" then
          return should, result
        elseif (type(should) == "number" or type(should) == "string") and
          (type(result) == "number" or type(result) == "string") then
          return tonumber(should), tonumber(result)
        else
          local tshould = nil
          local tresult = nil
          if type(should) == "table" then
            if type(should[1]) == "number" then
              tshould = should[1]
            elseif type(should[1]) == "string" then
              tshould = tonumber(should[1])
            end
          elseif type(should) == "string" then
            tshould = tonumber(should)
          elseif type(should) == "number" then
            tshould = should
          end

          if type(result) == "table" then
            if type(result[1]) == "number" then
              tresult = result[1]
            elseif type(result[1]) == "string" then
              tresult = tonumber(result[1])
            end
          elseif type(result) == "string" then
            tresult = tonumber(result)
          elseif type(result) == "number" then
            tresult = result
          end
          return tshould, tresult
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
      
      cmpmet.resemble = function(self,should,p)
        currentTest.try = function(result)
          local same = true
          if type(should) == "table" and type(result) == "table" then
            should_set = set.new(should)
            result_set = set.new(result)
            same = set.equal(should_set,result_set)
          elseif type(should) == "string" and type(result) == "string" then
            same = should:lower() == result:lower()
          elseif type(should) == "number" and type(result) == "number" then
            local precision = 0
            if type(p) == "number" then
              precision = p
            end
            same = math.round(should,precision) == math.round(result,precision)
          elseif type(should) == type(result) then
            same = should == result
          elseif (type(should) == "string" and type(result) == "number") or
            (type(result) == "string" and type(should) == "number") then
            if type(p) == "number" then
              same = math.round(tonumber(should),p) == math.round(tonumber(result),p)
            else
              same = tonumber(should) == tonumber(result)
            end
          else
            if type(should) == "table" then
              if type(should[1]) == "number" or type(result) == "number" then
                if type(p) == "number" then
                  same = math.round(tonumber(should[1]),p) == math.round(tonumber(result),p)
                else
                  same = tonumber(should[1]) == tonumber(result)
                end
              else
                same = tostring(should[1]):lower() == tostring(result):lower()
              end
            elseif type(result) == "table" then
              if type(result[1]) == "number" or type(should) == "number" then
                if type(p) == "number" then
                  same = math.round(tonumber(result[1]),p) == math.round(tonumber(should),p)
                else
                  same = tonumber(result[1]) == tonumber(should)
                end
              else
                same = tostring(result[1]):lower() == tostring(should):lower()
              end
            end
          end
          return self:report(" does resemble "," does not resemble ",same,should,result)
        end
      end
      
      cmpmet.be_more = function(self,should)
        currentTest.try = function(result)
          local tshould, tresult = self:numbers(should,result)
          if type(tshould) ~= "nil" and type(tresult) ~= "nil" then
            local more =  tresult > tshould
            return self:report(" is more than "," is not more than ", more,should,result)
          else
            return false, "Comparison data needs to be tables, numbers or strings: should is '"..type(should).."', result is '"..type(result).."'"
          end
        end
        return currentCase
      end
      
      cmpmet.be_less = function(self,should)
        currentTest.try = function(result)
          local tshould, tresult = self:numbers(should,result)
          if type(tshould) ~= "nil" and type(tresult) ~= "nil" then
            local less =  tresult < tshould
            return self:report(" is less than "," is not less than ", less,should,result)
          else
            return false, "Comparison data needs to be tables, numbers or strings: should is '"..type(should).."', result is '"..type(result).."'"
          end
        end
        return currentCase
      end
      
      cmpmet.be_more_or_equal = function(self,should)
        currentTest.try = function(result)
          local tshould, tresult = self:numbers(should,result)
          if type(tshould) ~= "nil" and type(tresult) ~= "nil" then
            local more =  tresult >= tshould
            return self:report(" is more or equal than "," is not more or equal than ", more,should,result)
          else
            return false, "Comparison data needs to be tables, numbers or strings: should is '"..type(should).."', result is '"..type(result).."'"
          end
        end
        return currentCase
      end
      
      cmpmet.be_less_or_equal = function(self,should)
        currentTest.try = function(result)
          local tshould, tresult = self:numbers(should,result)
          if type(tshould) ~= "nil" and type(tresult) ~= "nil" then
            local less =  tresult <= tshould
            return self:report(" is less or equal than "," is not less or equal than ", less,should,result)
          else
            return false, "Comparison data needs to be tables, numbers or strings: should is '"..type(should).."', result is '"..type(result).."'"
          end
        end
        return currentCase
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
      
      cmpmet.be_any = function(self,should)
        currentTest.try = function(result)
          local same = true
          if type(should) == "table"  and
            #should > 0 and type(should[1]) == "table" and
            #should[1] > 0 and type(result) == "table" then
            for i,v in ipairs(should) do
              for si,sv in ipairs(v) do
                if sv ~= result[si] then
                  same = false
                end
              end
              if same then
                break
              elseif i < #should then
                same = true
              end
            end
          elseif type(should) == "table"  and #should > 0 and type(should[1]) == "string" then
            local result_value = ""
            if type(result) == "string" then
              result_value = result
            elseif type(result) == "number" then
              result_value = tostring(result)
            elseif type(result) == "table" and #result == 1 then
              result_value = tostring(result[1])
            else
              return false, "Comparison result table needs to have only one member to be comparable with 'be_any': result is '"..type(result).."' of length "..tostring(#result)..""
            end
            local should_set = set.new(should)
            same = set.member(should_set,result_value)
          elseif type(should) == "table"  and #should > 0 and type(should[1]) == "number" then
            local result_value = 0
            if type(result) == "string" then
              result_value = tonumber(result)
            elseif type(result) == "number" then
              result_value = result
            elseif type(result) == "table" and #result == 1 then
              result_value = tonumber(result[1])
            else
              return false, "Comparison result table needs to have only one member to be comparable with 'be_any': result is '"..type(result).."' of length "..tostring(#result)..""
            end
            local should_set = set.new(should)
            same = set.member(should_set,result_value)
          else
            return false, "Comparison data needs to be table: should is '"..type(should).."'"
          end
          return self:report(" is any "," is not any one ",same,should,result)
        end
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
  
  table.insert(_pdtest.queue,currentSuite)
  return currentSuite
end

_pdtest.next = function()
  if table.getn(_pdtest.queue) == 0 then
    return false
  elseif table.getn(_pdtest.queue[1].queue) == 0 then
    table.insert(_pdtest.dones, table.remove(_pdtest.queue,1))
  elseif table.getn(_pdtest.queue[1].queue[1].queue) == 0 then
    table.insert(_pdtest.queue[1].dones, table.remove(_pdtest.queue[1].queue,1))
  else
    local current = _pdtest.queue[1].queue[1].queue[1]
    current.suite.before()
    current.case.before()
    if type(current.test) == "function" then
      current.name = "function"
      current.test(_pdtest.test)
    elseif type(current.test) == "table" then
      current.name = table.concat(current.test, ", ")
      _pdtest.test(current.test)
    elseif type(current.test) == "string" then
      current.name = current.test
      _pdtest.test(current.test)
    elseif type(current.test) == "number" then
      current.name = tostring(current.test)
      _pdtest.test(current.test)
    else
      _.error("wrong test data type -- "..type(current.test).." -- should have been function or table")
    end
    current.suite.after()
    current.case.after()
    table.insert(_pdtest.currents, current)
    table.insert(_pdtest.queue[1].queue[1].dones, table.remove(_pdtest.queue[1].queue[1].queue,1))
  end
  return true
end

_pdtest.yield = function()
  if table.getn(_pdtest.currents) > 0 and table.getn(_pdtest.results) > 0 then
    local current = table.remove(_pdtest.currents,1)
    local result = table.remove(_pdtest.results,1)
    current.result = result
    current.success, current.detail = current.try(current.result)
    local nametag = ""..current.suite.name.." -> "..current.case.name.." >> "..current.detail
    if current.success then
      _.post(nametag)
      _.post("-> OK")
    else
      _.post(nametag)
      _.post("x> FAILED")
    end
  end
  
  if table.getn(_pdtest.currents) == 0 and table.getn(_pdtest.results) == 0 and table.getn(_pdtest.queue) == 0 then
    return false
  end
  return true
end

_pdtest.report = function()
  if table.getn(_pdtest.currents) > 0 then
    return false
  else
    _.post("!!! Test Completed !!!")
    local tests = 0
    local cases = 0
    local suites = 0
    local ok = 0
    local fail = 0
    for si, suite in ipairs(_pdtest.dones) do
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
    _.post(""..suites.." Suites | "..cases.." Cases | "..tests.." Tests")
    _.post(""..ok.." tests passed")
    _.post(""..fail.." tests failed")
  end
  return true
end
