/*

Copyright (C) 2011 by Louis-Philippe Perron

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

/* for reading files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Lua headers */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "m_pd.h"

#define PDTEST_MAJOR 0
#define PDTEST_MINOR 3
#define PDTEST_PATCH 0
#define PD_MAJOR_VERSION 0
#define PD_MINOR_VERSION 42

static t_class *pdtest_class;
typedef struct pdtest
{
  t_object x_obj;
  lua_State *lua;
  t_canvas *canvas;
  int async_num;
  int async_run;
  t_clock  *async_clock;
} t_pdtest;

/* Declarations */

/* pdtest init methods */
void pdtest_setup(void);
void *pdtest_new(void);
void pdtest_free(t_pdtest *x);

/* pdtest inlets methods */
void pdtest_suite(t_pdtest *x, t_symbol *s, int argc, t_atom *argv);
void pdtest_result(t_pdtest *x, t_symbol *s, int argc, t_atom *argv);

/* pdtest scheduler method */
static void pdtest_run(t_pdtest *x);
static void pdtest_next(t_pdtest * x);
static void pdtest_yield(t_pdtest * x);
static void pdtest_schedule(t_pdtest *x);
void pdtest_start(t_pdtest *x, t_symbol *s);
void pdtest_stop(t_pdtest *x, t_symbol *s);

/* helper functions */
void pdtest_luasetup(t_pdtest *x);
static const char *pdtest_lua_reader(lua_State *lua, void *rr, size_t *size);
void pdtest_lua_loadsuite(t_pdtest *x, const char *filename);
static t_atom *pdtest_lua_popatomtable(lua_State *L, int *count);
static void pdtest_report(t_pdtest *x);

/* Lua functions */
static int luafunc_pdtest_message(lua_State *lua);
static int luafunc_pdtest_error(lua_State *lua);
static int luafunc_pdtest_post(lua_State *lua);
static int luafunc_pdtest_errorHandler(lua_State *lua);

/* Lua source code */
const char* pdtest_lua_init;
const char* pdtest_lua_next;
const char* pdtest_lua_yield;
const char* pdtest_lua_report;

void pdtest_setup(void)
{
    pdtest_class = class_new(gensym("pdtest"),
        (t_newmethod)pdtest_new,
        (t_method)pdtest_free,
        sizeof(t_pdtest),
        CLASS_DEFAULT,0);
    
    class_addmethod(pdtest_class,
        (t_method)pdtest_suite, gensym("suite"),
        A_GIMME, 0);
    class_addmethod(pdtest_class,
        (t_method)pdtest_result, gensym("result"),
        A_GIMME, 0);
    class_addmethod(pdtest_class,
        (t_method)pdtest_start, gensym("start"),0);
    class_addmethod(pdtest_class,
        (t_method)pdtest_stop, gensym("stop"),0);
    
    class_sethelpsymbol(pdtest_class, gensym("pdtest-help"));
    
    post("pdtest %i.%i.%i (MIT) 2011 Louis-Philippe Perron <lp@spiralix.org>", PDTEST_MAJOR, PDTEST_MINOR, PDTEST_PATCH);
    post("pdtest: compiled for pd-%d.%d on %s %s", PD_MAJOR_VERSION, PD_MINOR_VERSION, __DATE__, __TIME__);
}

void *pdtest_new(void)
{
    t_pdtest *x = NULL;
    x = (t_pdtest*)pd_new(pdtest_class);
    
    x->canvas = canvas_getcurrent();
    pdtest_luasetup(x);
    
    x->async_run = 0;
    x->async_clock = clock_new(x, (t_method)pdtest_run);
    
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("list"), gensym("result"));
    outlet_new(&x->x_obj, NULL);
    
    return (void*)x;
}

void pdtest_free(t_pdtest *x)
{
    lua_close(x->lua);
}

/* pdtest inlets methods */

void pdtest_suite(t_pdtest *x, t_symbol *s, int argc, t_atom *argv)
{
    if (argc < 1) {
        post("pdtest: at least one suite filepath needed"); return;
    }
    post("*** suite");
    int i;
    for (i = 0; i < argc; i++) {
        char filepath[256];
        atom_string(argv+i, filepath, 256);
        const char* filestring = (const char*)filepath;
        post("pdtest: loading testfile %s", filestring);
        
        pdtest_lua_loadsuite(x, filestring);
    }
    post("*** suite loaded...  starting!");
    pdtest_start(x,gensym("suite"));
}

void pdtest_result(t_pdtest *x, t_symbol *s, int argc, t_atom *argv)
{
    post("*** result");
  if (argc < 1) {
      post("pdtest: result is empty"); return;
  }
  
  lua_getglobal(x->lua, "pdtest");
  lua_getfield(x->lua, -1, "results");
  int n = luaL_getn(x->lua,-1);
  
  lua_newtable(x->lua);
  post("*** result table set");
  int i;
  for (i = 0; i < argc; i++) {
      char result[256];
      atom_string(argv+i, result, 256);
      const char* resultS = (const char*)result;
      
      if (!i == 0) {
          lua_pushstring(x->lua,resultS);
          lua_rawseti(x->lua,-2,i);
          
          post("*** result table + %s at index: %d",resultS, i);
      }
  }
  lua_rawseti(x->lua,-2,n+1);
}

/* pdtest scheduling methods */

static void pdtest_run(t_pdtest *x)
{
    post("*** run");
    pdtest_next(x);
    pdtest_yield(x);
    pdtest_schedule(x);
}

static void pdtest_next(t_pdtest * x)
{
    post("*** next");
    lua_getglobal(x->lua,"pdtest_errorHandler");
    lua_getglobal(x->lua, "pdtest_next");
    if (!lua_isfunction(x->lua,-1)) {
        post("*** gosh... no pdtest_next function!!!");
        return;
    }
    post("*** passed func!!!");
    int error = lua_pcall(x->lua, 0, 1, 0);
    post("*** next error: %d", error);
    post("*** run = %d; mem = %d; err = %d", LUA_ERRRUN, LUA_ERRMEM, LUA_ERRERR);
    if (!error) {
        if (lua_isboolean(x->lua,-1)) {
            int doing = lua_toboolean(x->lua,-1);
            if (!doing) {
                post("*** next no action");
            }
        }
    }
}

static void pdtest_yield(t_pdtest * x)
{
    lua_getglobal(x->lua,"pdtest_errorHandler");
    lua_getglobal(x->lua, "pdtest_yield");
    int error = lua_pcall(x->lua, 0, 1, -2);
    if (!error) {
        if (lua_isboolean(x->lua,-1)) {
            int again = lua_toboolean(x->lua,-1);
            if (!again) {
                pdtest_stop(x, gensym("yield"));
                pdtest_report(x);
            }
        }
    }
}

static void pdtest_schedule(t_pdtest *x)
{
    post("*** schedule");
    if (x->async_run) {
        clock_delay(x->async_clock, 0);
    } else {
        clock_unset(x->async_clock);
    }
}

void pdtest_start(t_pdtest *x, t_symbol *s)
{
    post("*** start");
    if (!x->async_run) {
      x->async_run = 1;
      pdtest_schedule(x);
    }
}

void pdtest_stop(t_pdtest *x, t_symbol *s)
{
    post("*** stop");
    if (x->async_run) {
      x->async_run = 0;
      pdtest_schedule(x);
    }
}

/*************************************************
 *               Helper functions                *
 *                                               *
 *************************************************/

void pdtest_luasetup(t_pdtest *x)
{
    x->lua = lua_open();
    luaL_openlibs(x->lua);
    
    lua_newtable(x->lua);
    lua_pushcfunction(x->lua, luafunc_pdtest_message);
    lua_setfield(x->lua, 1, "message");
    lua_pushcfunction(x->lua, luafunc_pdtest_error);
    lua_setfield(x->lua, 1, "error");
    lua_pushcfunction(x->lua, luafunc_pdtest_post);
    lua_setfield(x->lua, 1, "post");
    lua_setglobal(x->lua,"pdtest");
    
    lua_pushcfunction(x->lua, luafunc_pdtest_errorHandler);
    lua_setglobal(x->lua,"pdtest_errorHandler");
    
    lua_pushlightuserdata(x->lua, x);
    lua_setglobal(x->lua,"pdtest_userdata");
    
    luaL_dostring(x->lua, pdtest_lua_init);
    luaL_dostring(x->lua, pdtest_lua_next);
    luaL_dostring(x->lua, pdtest_lua_yield);
    luaL_dostring(x->lua, pdtest_lua_report);
}

/* State for the Lua file reader. */
typedef struct pdtest_lua_readerdata {
  int fd; /**< File descriptor to read from. */
  char buffer[MAXPDSTRING]; /**< Buffer to read into. */
} t_pdtest_lua_readerdata;

/* Lua file reader callback. */
static const char *pdtest_lua_reader(lua_State *lua, void *rr, size_t *size)
{
  t_pdtest_lua_readerdata *r = rr;
  size_t s;
  s = read(r->fd, r->buffer, MAXPDSTRING-2);
  if (s <= 0) {
    *size = 0;
    return NULL;
  } else {
    *size = s;
    return r->buffer;
  }
}

/* Load Lua test suite */
void pdtest_lua_loadsuite(t_pdtest *x, const char *filename)
{
  char buf[MAXPDSTRING];
  char *ptr;
  t_pdtest_lua_readerdata reader;
  int fd;
  int n;
  
  fd = canvas_open(x->canvas, filename, "", buf, &ptr, MAXPDSTRING, 1);
  if (fd >= 0) {
    reader.fd = fd;
    if (lua_load(x->lua, pdtest_lua_reader, &reader, filename)) {
      close(fd);
      pd_error(x, "pdtest: error loading testfile `%s':\n%s", filename, lua_tostring(x->lua, -1));
    } else {
      if (lua_pcall(x->lua, 0, 1, 0)) {
        pd_error(x, "pdtest: error loading testfile `%s':\n%s", filename, lua_tostring(x->lua, -1));
        lua_pop(x->lua, 1);
        close(fd);
      } else {
        /* succeeded */
        close(fd);
      }
    }
  } else {
    pd_error(x, "pdtest: error loading testfile `%s':\n%s", filename, "File Not Found!!?");
  }
}

/* Convert a Lua table into a Pd atom array. */
static t_atom *pdtest_lua_popatomtable(lua_State *L, int *count)
{
  int i = 0;
  int ok = 1;
  t_float f;
  const char *s;
  void *p;
  size_t sl;
  t_atom *atoms;
  atoms = NULL;
  
  if (lua_istable(L, -1)) {
    *count = lua_objlen(L, -1);
    if (*count > 0) { atoms = malloc(*count * sizeof(t_atom)); }
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
      if (i == *count) {
        error("pdtest: too many table elements in message");
        ok = 0;
        break;
      }
      switch (lua_type(L, -1)) {
      case (LUA_TNUMBER):
        f = lua_tonumber(L, -1);
        SETFLOAT(&atoms[i], f);
        break;
      case (LUA_TSTRING):
        s = lua_tolstring(L, -1, &sl);
        if (s) {
          if (strlen(s) != sl) {
            error("pdtest: message symbol munged (contains \\0 in body)");
          }
          SETSYMBOL(&atoms[i], gensym((char *) s));
        } else {
          error("pdtest: null string in message table");
          ok = 0;
        }
        break;
      case (LUA_TLIGHTUSERDATA): /* FIXME: check experimentality */
        p = lua_touserdata(L, -1);
        SETPOINTER(&atoms[i], p);
        break;
      default:
        error("pdtest: message table element must be number or string or pointer");
        ok = 0;
        break;
      }
      lua_pop(L, 1);
      ++i;
    }
    if (i != *count) {
      error("pdtest: too few message table elements");
      ok = 0;
    }
  } else {
    error("pdtest: pdtest.message function takes a table as its only argument");
    ok = 0;
  }
  lua_pop(L, 1);
  if (ok) {
    return atoms;
  } else {
    if (atoms) {
      free(atoms);
    }
    return NULL;
  }
}

static void pdtest_report(t_pdtest *x)
{
    lua_getglobal(x->lua,"pdtest_errorHandler");
    lua_getglobal(x->lua, "pdtest_report");
    int error = lua_pcall(x->lua, 0, 1, -2);
    if (!error) {
        if (lua_isboolean(x->lua,-1)) {
            int done = lua_toboolean(x->lua,-1);
            if (!done) {
                post("pdtest: postponing report as the queue is not empty...");
                pdtest_start(x,gensym("report"));
            }
        }
    }
}

/*************************************************
 *               Lua functions                   *
 *                                               *
 *************************************************/

static int luafunc_pdtest_message(lua_State *lua)
{
    post("*** message");
    int count;
    t_atom *message = pdtest_lua_popatomtable(lua,&count);
    
    lua_getglobal(lua, "pdtest_userdata");
    if (lua_islightuserdata(lua,-1)) {
      post("*** message to out");
      t_pdtest *x = lua_touserdata(lua,-1);
      outlet_list(x->x_obj.ob_outlet, &s_list, count, &message[0]);
    } else {
      post("*** message userdata missing");
      error("pdtest: userdata missing from Lua stack");
    }
    post("*** message done!");
    return 1;
}

static int luafunc_pdtest_error(lua_State *lua)
{
  const char *s = luaL_checkstring(lua, 1);
  if (s) {
    error("pdtest: %s", s);
  } else {
    error("pdtest: error in posting pd error, no error message string in Lua stack");
  }
  
  return 0;
}

static int luafunc_pdtest_post(lua_State *lua)
{
  const char *s = luaL_checkstring(lua, 1);
  if (s) {
    post("pdtest: %s", s);
  } else {
    error("pdtest: error in posting pd post, no message string in Lua stack");
  }
  
  return 0;
}

static int luafunc_pdtest_errorHandler(lua_State *lua)
{
    const char* err;
    if (lua_isstring(lua,-1)) {
        err = lua_tostring(lua,-1);
    } else {
        err = " ";
    }
    error("pdtest: lua error: %s", err);
    return 1;
}



/*************************************************
 *               Lua Source Code                 *
 *                                               *
 *************************************************/

const char* pdtest_lua_init = "\n"
"pdtest.queue={}\n"
"pdtest.dones={}\n"
"pdtest.results={}\n"
"pdtest.currents={}\n"
"pdtest.try = 0;\n"
"pdtest.suite = function(suite)\n"
"  currentSuite = {name=suite, queue={}, dones={}}\n"
"  \n"
"  currentSuite.case = function(case)\n"
"    currentCase = {name=case, queue={}, dones={}}\n"
"    \n"
"    currentCase.before = function() end\n"
"    currentCase.after = function() end\n"
"    currentCase.setup = function(setup)\n"
"      if type(setup) == \"function\" then\n"
"        currentCase.before = setup\n"
"        return currentCase\n"
"      end\n"
"    end\n"
"    currentCase.teardown = function(teardown)\n"
"      if type(teardown) == \"function\" then\n"
"        currentCase.after = teardown\n"
"        return currentCase\n"
"      end\n"
"    end\n"
"    \n"
"    currentCase.test = function(test)\n"
"      currentTest = {test=test, case=currentCase}\n"
"      \n"
"      currentTest.should = function()\n"
"        cmpmet = {}\n"
"        cmpmet.equal = function(should)\n"
"          currentTest.try = function(result)\n"
"            if type(should) == \"table\" and type(result) == \"table\" then\n"
"              same = true\n"
"              for i,v in ipairs(should) do\n"
"                if v ~= result[i] then\n"
"                  same = false\n"
"                end\n"
"              end\n"
"              if same then\n"
"                return true, \"\"..table.concat(should, \", \")..\" is equal to \"..table.concat(result,\", \")..\"\"\n"
"              else\n"
"                return false, \"\"..table.concat(should, \", \")..\" is not equal to \"..table.concat(result,\", \")..\"\"\n"
"              end\n"
"            else\n"
"              return false, \"Comparison data needs to be tables: should is '\"..type(should)..\"', result is '\"..type(result)..\"'\"\n"
"            end\n"
"          end\n"
"          return currentTest\n"
"        end\n"
"        \n"
"        return cmpmet\n"
"      end\n"
"      \n"
"      table.insert(currentCase.queue,currentTest)\n"
"      return currentTest\n"
"    end\n"
"    \n"
"    table.insert(currentSuite.queue,currentCase)\n"
"    return currentCase\n"
"  end\n"
"  \n"
"  table.insert(pdtest.queue,currentSuite)\n"
"  return currentSuite\n"
"end\n";

const char* pdtest_lua_next = "\n"
"function pdtest_next()\n"
"  pdtest.post(\"*** lua next\")\n"
"  if table.getn(pdtest.queue) == 0 then\n"
"    pdtest.post(\"*** lua next no suites\")\n"
"    return false\n"
"  elseif table.getn(pdtest.queue[1].queue) == 0 then\n"
"    pdtest.post(\"*** lua next no cases\")\n"
"    table.insert(pdtest.dones, table.remove(pdtest.queue,1))\n"
"  elseif table.getn(pdtest.queue[1].queue[1].queue) == 0 then\n"
"    pdtest.post(\"*** lua next no tests\")\n"
"    table.insert(pdtest.queue[1].dones, table.remove(pdtest.queue[1].queue,1))\n"
"  else\n"
"    pdtest.post(\"*** lua next test\")\n"
"    current = pdtest.queue[1].queue[1].queue[1]\n"
"    current.case.setup()\n"
"    if type(current.test) == \"function\" then\n"
"      pdtest.post(\"*** lua next test function\")\n"
"      current.test()\n"
"    elseif type(current.test) == \"table\" then\n"
"      pdtest.post(\"*** lua next test message\")\n"
"      pdtest.message(current.test)\n"
"    else\n"
"      pdtest.error(\"wrong test data type -- \"..type(current.test)..\" -- should have been function or table\")\n"
"    end\n"
"    current.case.teardown()\n"
"    table.insert(pdtest.currents, current)\n"
"    table.insert(pdtest.queue[1].queue[1].dones, table.remove(pdtest.queue[1].queue[1].queue,1))\n"
"  end\n"
"  return true\n"
"end\n";

const char* pdtest_lua_yield = "\n"
"function pdtest_yield()\n"
"  pdtest.post(\"lua yield\")\n"
"  if table.getn(pdtest.currents) > 0 and table.getn(pdtest.results) > 0 then\n"
"    current = table.remove(pdtest.currents,1)\n"
"    result = table.remove(pdtest.results,1)\n"
"    current.result = result\n"
"    current.success, current.detail = current.try(current.result)\n"
"    if current.success then\n"
"      pdtest.post(\"-OK\")\n"
"    else\n"
"      pdtest.post(\"-FAILED: \"..current.detail)\n"
"    end\n"
"    return true\n"
"  elseif table.getn(pdtest.currents) == 0 and table.getn(pdtest.results) == 0 and table.getn(pdtest.queue) == 0 then\n"
"    return false\n"
"  end\n"
"end\n";

const char* pdtest_lua_report = "\n"
"function pdtest_report()\n"
"  if table.getn(pdtest.currents) > 0 then\n"
"    return false\n"
"  else\n"
"    pdtest.post(\"!!! Test Completed !!!\")\n"
"    tests = 0\n"
"    cases = 0\n"
"    suites = 0\n"
"    ok = 0\n"
"    fail = 0\n"
"    for si, suite in ipairs(pdtest.dones) do\n"
"      suites = suites + 1\n"
"      for ci, case in ipairs(suite.dones) do\n"
"        cases = cases + 1\n"
"        for ti, test in ipairs(case.dones) do\n"
"          tests = tests + 1\n"
"          if test.success then\n"
"            ok = ok + 1\n"
"          else\n"
"            fail = fail + 1\n"
"          end\n"
"        end\n"
"      end\n"
"    end\n"
"    pdtest.post(\"\"..suites..\" Suites | \"..cases..\" Cases | \"..tests..\" Tests\")\n"
"    pdtest.post(\"\"..ok..\" tests passed\")\n"
"    pdtest.post(\"\"..fail..\" tests failed\")\n"
"  end\n"
"  return true\n"
"end\n";


