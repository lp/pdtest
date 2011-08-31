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
#include <sys/time.h>

/* Lua headers */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "m_pd.h"

#define PDTEST_MAJOR 0
#define PDTEST_MINOR 5
#define PDTEST_PATCH 5
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
void pdtest_start(t_pdtest *x, t_symbol *s);
void pdtest_stop(t_pdtest *x, t_symbol *s);
void pdtest_reset(t_pdtest *x, t_symbol *s);

/* pdtest scheduler method */
static void pdtest_run(t_pdtest *x);
static void pdtest_next(t_pdtest * x);
static void pdtest_yield(t_pdtest * x);
static void pdtest_schedule(t_pdtest *x);

/* helper functions */
void pdtest_luasetup(t_pdtest *x);
static const char *pdtest_lua_reader(lua_State *lua, void *rr, size_t *size);
void pdtest_lua_loadsuite(t_pdtest *x, const char *filename);
static t_atom *pdtest_lua_popatomtable(lua_State *L, int *count);
static void pdtest_report(t_pdtest *x);
static int pdtest_is_rawmessage(t_pdtest *x);
static void pdtest_reg_message(t_pdtest *x, int israw);

/* Lua functions */
static int luafunc_pdtest_message(lua_State *lua);
static int luafunc_pdtest_raw_message(lua_State *lua);
static int luafunc_pdtest_error(lua_State *lua);
static int luafunc_pdtest_post(lua_State *lua);
static int luafunc_pdtest_errorHandler(lua_State *lua);
static int luafunc_pdtest_register(lua_State *lua);
static int luafunc_pdtest_unregister(lua_State *lua);

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
    class_addmethod(pdtest_class,
        (t_method)pdtest_reset, gensym("reset"),0);
    
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
    (void) s; /* get rid of parameter warning */
    if (argc < 1) {
        error("pdtest: at least one suite filepath needed"); return;
    }
    int i;
    for (i = 0; i < argc; i++) {
        char filepath[256];
        atom_string(argv+i, filepath, 256);
        const char* filestring = (const char*)filepath;
        post("pdtest: loading testfile %s", filestring);
        
        pdtest_lua_loadsuite(x, filestring);
    }
    pdtest_start(x,gensym("suite"));
}

void pdtest_result(t_pdtest *x, t_symbol *s, int argc, t_atom *argv)
{
  (void) s; /* get rid of parameter warning */
  if (argc < 1) {   /* skip result was empty ??? */
      error("pdtest: result message is empty"); return;}
  /* skip result if correspondent message was raw_message */
  if (pdtest_is_rawmessage(x)) { return; }
  
  /* gets pdtest.results table onto the stack*/
  lua_getglobal(x->lua, "pdtest");
  lua_getfield(x->lua, -1, "results");
  int n = luaL_getn(x->lua,-1);
  
  /* build result message table from argv */
  lua_newtable(x->lua);
  int i;
  for (i = 0; i < argc; i++) {
      char result[256];
      atom_string(argv+i, result, 256);
      const char* resultstring = (const char*)result;
      
      if (!i == 0) {
          lua_pushstring(x->lua,resultstring);
          lua_rawseti(x->lua,-2,i);
      }
  }
  
  lua_rawseti(x->lua,-2,n+1);   /* push result message in pdtest.table */
  lua_pop(x->lua,2);            /* clean up the stack */
}

void pdtest_start(t_pdtest *x, t_symbol *s)
{
    (void) s; /* get rid of parameter warning */
    if (!x->async_run) {
      x->async_run = 1;
      pdtest_schedule(x);
    }
}

void pdtest_stop(t_pdtest *x, t_symbol *s)
{
    (void) s; /* get rid of parameter warning */
    if (x->async_run) {
      x->async_run = 0;
      pdtest_schedule(x);
    }
}

void pdtest_reset(t_pdtest *x, t_symbol *s)
{
    (void) s; /* get rid of parameter warning */
    
    /* set empty tables as new test containers */
    lua_getglobal(x->lua,"pdtest");
    lua_newtable(x->lua);
    lua_setfield(x->lua, -2, "queue");
    lua_newtable(x->lua);
    lua_setfield(x->lua, -2, "dones");
    lua_newtable(x->lua);
    lua_setfield(x->lua, -2, "results");
    lua_newtable(x->lua);
    lua_setfield(x->lua, -2, "currents");
    lua_pop(x->lua,1);    /* clean up the stack */
}

/* pdtest scheduling methods */

static void pdtest_run(t_pdtest *x)
{
    pdtest_next(x);
    pdtest_yield(x);
    pdtest_schedule(x);
}

static void pdtest_next(t_pdtest * x)
{
    /* prepares the stack to call pdtest.next() */
    lua_getglobal(x->lua,"pdtest_errorHandler");
    lua_getglobal(x->lua, "pdtest_next");
    if (!lua_isfunction(x->lua,-1)) {
      lua_pop(x->lua,2);
      error("pdtest: no pdtest_next function!!!");
      return;
    }
    int err = lua_pcall(x->lua, 0, 1, 0);
    
    if (lua_isfunction(x->lua,-2))    /* clean error handler from stack */
      lua_remove(x->lua,-2);
    
    /* if no pcall error, check if still has job and signal error if not */
    if (!err) {   
        if (lua_isboolean(x->lua,-1)) {
            int doing = lua_toboolean(x->lua,-1);
            if (!doing) { error("pdtest: no tests to run..."); }
        } else { error("pdtest: pdtest.next() didn't return a bool??"); }
        lua_pop(x->lua,1);  /* clean stack from bool result */
    } 
}

static void pdtest_yield(t_pdtest * x)
{
    /* prepares the stack to call pdtest.yield() */
    lua_getglobal(x->lua,"pdtest_errorHandler");
    lua_getglobal(x->lua, "pdtest_yield");
    if (!lua_isfunction(x->lua,-1)) {
      lua_pop(x->lua,2);
      error("pdtest: no pdtest_yield function!!!");
      return;
    }
    int err = lua_pcall(x->lua, 0, 1, -2);
    
    if (lua_isfunction(x->lua,-2))    /* clean error handler from stack */
      lua_remove(x->lua,-2);
    
    /* if no pcall error, check if still has results and stops if not */
    if (!err) {
        if (lua_isboolean(x->lua,-1)) {
            int again = lua_toboolean(x->lua,-1);
            if (!again) {
                pdtest_stop(x, gensym("yield"));
                pdtest_report(x);
            }
        } else { error("pdtest: pdtest.yield() didn't return a bool??"); }
        lua_pop(x->lua,1);  /* clean stack from bool result */
    }
}

static void pdtest_schedule(t_pdtest *x)
{
    if (x->async_run) {
        clock_delay(x->async_clock, 0);
    } else {
        clock_unset(x->async_clock);
    }
}

/*************************************************
 *               Helper functions                *
 *                                               *
 *************************************************/

void pdtest_luasetup(t_pdtest *x)
{
    /* gets new lua state and loads standard libs */
    x->lua = lua_open();
    luaL_openlibs(x->lua);
    
    /* prepare global pdtest namespace */
    lua_newtable(x->lua);
    lua_pushcfunction(x->lua, luafunc_pdtest_message);
    lua_setfield(x->lua, -2, "message");
    lua_pushcfunction(x->lua, luafunc_pdtest_raw_message);
    lua_setfield(x->lua, -2, "raw_message");
    lua_pushcfunction(x->lua, luafunc_pdtest_error);
    lua_setfield(x->lua, -2, "error");
    lua_pushcfunction(x->lua, luafunc_pdtest_post);
    lua_setfield(x->lua, -2, "post");
    lua_pushcfunction(x->lua, luafunc_pdtest_register);
    lua_setfield(x->lua, -2, "register");
    lua_pushcfunction(x->lua, luafunc_pdtest_unregister);
    lua_setfield(x->lua, -2, "unregister");
    lua_newtable(x->lua);
    lua_setfield(x->lua, -2, "queue");
    lua_newtable(x->lua);
    lua_setfield(x->lua, -2, "dones");
    lua_newtable(x->lua);
    lua_setfield(x->lua, -2, "results");
    lua_newtable(x->lua);
    lua_setfield(x->lua, -2, "currents");
    lua_newtable(x->lua);
    lua_setfield(x->lua, -2, "reg");
    lua_setglobal(x->lua,"pdtest");
    
    /* set error handler and t_pdtest instance in global namespace */
    lua_register(x->lua,"pdtest_errorHandler",luafunc_pdtest_errorHandler);
    lua_pushlightuserdata(x->lua, x);
    lua_setglobal(x->lua,"pdtest_userdata");
    
    /* execute lua source code embeded in C strings to initialize lua test system */
    int err;
    err = luaL_dostring(x->lua, pdtest_lua_init);
    if (err) {
        if (lua_isstring(x->lua,-1))
            error("error loading 'pdtest_lua_init': %s", lua_tostring(x->lua,-1));
    }
    err = luaL_dostring(x->lua, pdtest_lua_next);
    if (err) {
        if (lua_isstring(x->lua,-1))
            error("error loading 'pdtest_lua_next': %s", lua_tostring(x->lua,-1));
    }
    err = luaL_dostring(x->lua, pdtest_lua_yield);
    if (err) {
        if (lua_isstring(x->lua,-1))
            error("error loading 'pdtest_lua_yield': %s", lua_tostring(x->lua,-1));
    }
    err = luaL_dostring(x->lua, pdtest_lua_report);
    if (err) {
        if (lua_isstring(x->lua,-1))
            error("error loading 'pdtest_lua_report': %s", lua_tostring(x->lua,-1));
    }
}

/* State for the Lua file reader. */
/* partially borrowed from pdlua */
typedef struct pdtest_lua_readerdata {
  int fd; /**< File descriptor to read from. */
  char buffer[MAXPDSTRING]; /**< Buffer to read into. */
} t_pdtest_lua_readerdata;

/* Lua file reader callback. */
/* partially borrowed from pdlua */
static const char *pdtest_lua_reader(lua_State *lua, void *rr, size_t *size)
{
  (void) lua; /* get rid of unused parameters warning */
  t_pdtest_lua_readerdata *r = rr; size_t s;
  s = read(r->fd, r->buffer, MAXPDSTRING-2);
  if (s <= 0) {
    *size = 0; return NULL;
  } else {
    *size = s; return r->buffer;
  }
}

/* Load Lua test suite */
/* partially borrowed from pdlua */
void pdtest_lua_loadsuite(t_pdtest *x, const char *filename)
{
  char buf[MAXPDSTRING]; char *ptr;
  t_pdtest_lua_readerdata reader;
  int fd;
  fd = canvas_open(x->canvas, filename, "", buf, &ptr, MAXPDSTRING, 1);
  if (fd >= 0) {
    reader.fd = fd;
    if (lua_load(x->lua, pdtest_lua_reader, &reader, filename)) {
      close(fd);
      error("pdtest: error loading testfile `%s':\n%s", filename, lua_tostring(x->lua, -1));
    } else {
      if (lua_pcall(x->lua, 0, 1, 0)) {
        error("pdtest: error loading testfile `%s':\n%s", filename, lua_tostring(x->lua, -1));
        lua_pop(x->lua, 1);
        close(fd);
      } else {
        /* succeeded */
        close(fd);
      }
    }
  } else {
    error("pdtest: error loading testfile `%s':\n%s", filename, "File Not Found!!?");
  }
}

/* Convert a Lua table into a Pd atom array. */
/* partially borrowed from pdlua */
static t_atom *pdtest_lua_popatomtable(lua_State *L, int *count)
{
  int i = 0; int ok = 1; t_float f;
  const char *s; void *p; size_t sl;
  t_atom *atoms; atoms = NULL;
  
  if (lua_istable(L, -1)) {
    *count = lua_objlen(L, -1);
    if (*count > 0) { atoms = malloc(*count * sizeof(t_atom)); }
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
      if (i == *count) {
        error("pdtest: too many table elements in message");
        ok = 0; break;
      }
      switch (lua_type(L, -1)) {
      case (LUA_TNUMBER):
        f = lua_tonumber(L, -1);
        SETFLOAT(&atoms[i], f); break;
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
        ok = 0; break;
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
    /* prepares the stack to call pdtest.report() */
    lua_getglobal(x->lua,"pdtest_errorHandler");
    lua_getglobal(x->lua, "pdtest_report");
    if (!lua_isfunction(x->lua,-1)) {
      lua_pop(x->lua,2);
      error("pdtest: no pdtest_report function!!!");
      return;
    }
    int err = lua_pcall(x->lua, 0, 1, -2);
    if (lua_isfunction(x->lua,-2))    /* clean error handler from stack */
      lua_remove(x->lua,-2);
    
    /* if no pcall error, check if still has results and re-start if there is some */
    if (!err) {
        if (lua_isboolean(x->lua,-1)) {
            int done = lua_toboolean(x->lua,-1);
            if (!done) {
                post("pdtest: postponing report as the queue is not empty...");
                pdtest_start(x,gensym("report"));
            }
        } else { error("pdtest: pdtest.report() didn't return a bool??"); }
        lua_pop(x->lua,1);  /* clean stack from bool result */
    }
}

static int pdtest_is_rawmessage(t_pdtest *x)
{
    /* prepares the stack to call pdtest.unregister() */
    lua_getglobal(x->lua,"pdtest_errorHandler");
    lua_getglobal(x->lua,"pdtest");
    lua_getfield(x->lua, -1, "unregister");
    if (!lua_isfunction(x->lua,-1)) {
      lua_pop(x->lua,3);
      error("pdtest: no pdtest_unregister function!!!");
      return -1;
    }
    if (lua_istable(x->lua,-2))    /* clean pdtest table from stack */
      lua_remove(x->lua,-2);
    
    int err = lua_pcall(x->lua, 0,1,-2);
    if (lua_isfunction(x->lua,-2))    /* clean error handler from stack */
      lua_remove(x->lua,-2);
    
    /* if no pcall error, return pcall bool result */
    if (!err) {
      if (lua_isboolean(x->lua,-1)) {
          int result = lua_toboolean(x->lua,-1);
          lua_pop(x->lua,1);  /* clean stack from bool result */
          return result;
      } else {
          lua_pop(x->lua,1);  /* clean stack from bool result */
          error("pdtest unregister didn't return a bool...");
      }
    }
    return -1;
}

static void pdtest_reg_message(t_pdtest *x, int israw)
{
    /* prepares the stack to call pdtest.register() */
    lua_getglobal(x->lua,"pdtest_errorHandler");
    lua_getglobal(x->lua,"pdtest");
    lua_getfield(x->lua, -1, "register");
    if (!lua_isfunction(x->lua,-1)) {
        lua_pop(x->lua,3);
        error("pdtest: no pdtest_reg_message function!!!");
        return;
    }
    if (lua_istable(x->lua,-2))    /* clean pdtest table from stack */
      lua_remove(x->lua,-2);
    
    lua_pushboolean(x->lua, israw); /* push bool argument to register() */
    lua_pcall(x->lua,1,0,-3);
    if (lua_isfunction(x->lua,-1))    /* clean error handler from stack */
      lua_remove(x->lua,-1);
}

/*************************************************
 *               Lua functions                   *
 *                                               *
 *************************************************/

static int luafunc_pdtest_message(lua_State *lua)
{
    int count;
    t_atom *message = pdtest_lua_popatomtable(lua,&count);
    
    lua_getglobal(lua, "pdtest_userdata");
    if (lua_islightuserdata(lua,-1)) {
      t_pdtest *x = lua_touserdata(lua,-1);
      pdtest_reg_message(x,0);
      outlet_list(x->x_obj.ob_outlet, &s_list, count, &message[0]);
    } else {
      error("pdtest: userdata missing from Lua stack");
    }
    return 1;
}

static int luafunc_pdtest_raw_message(lua_State *lua)
{
    int count;
    t_atom *message = pdtest_lua_popatomtable(lua,&count);
    
    lua_getglobal(lua, "pdtest_userdata");
    if (lua_islightuserdata(lua,-1)) {
      t_pdtest *x = lua_touserdata(lua,-1);
      pdtest_reg_message(x,1);
      outlet_list(x->x_obj.ob_outlet, &s_list, count, &message[0]);
    } else {
      error("pdtest: userdata missing from Lua stack");
    }
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

static int luafunc_pdtest_register(lua_State *lua)
{
    lua_getglobal(lua,"pdtest");
    lua_getfield(lua, -1, "reg");
    int n = luaL_getn(lua,-1);
    lua_insert(lua,-3);
    lua_pop(lua,1);
    
    lua_rawseti(lua,-2,n+1);
    lua_pop(lua,1);
    return 0;
}

static int luafunc_pdtest_unregister(lua_State *lua)
{
    lua_getglobal(lua,"pdtest");
    lua_getfield(lua, -1, "reg");
    int n = luaL_getn(lua,-1);
    if (n == 0) {
        error("pdtest: unregistering on an empty queue?");
        lua_pushnil(lua);
        return 1;
    }
    luaL_setn(lua, -2, n-1);
    
    int pos = 1;
    lua_rawgeti(lua,-1,pos);
    for ( ;pos<n;pos++) {
        lua_rawgeti(lua, -2, pos+1);
        lua_rawseti(lua, -3, pos);
    }
    lua_pushnil(lua);
    lua_rawseti(lua, -3, n);
    
    lua_remove(lua, -3);
    lua_remove(lua, -2);
    
    return 1;
}

/*************************************************
 *               Lua Source Code                 *
 *                                               *
 *************************************************/

const char* pdtest_lua_init = "\n"
"pdtest.suite = function(suite)\n"
"  currentSuite = {name=suite, queue={}, dones={}}\n"
"  \n"
"  currentSuite.before = function() end\n"
"  currentSuite.after = function() end\n"
"  currentSuite.setup = function(setup)\n"
"    if type(setup) == \"function\" then\n"
"      currentSuite.before = setup\n"
"      return currentSuite\n"
"    end\n"
"  end\n"
"  currentSuite.teardown = function(teardown)\n"
"    if type(teardown) == \"function\" then\n"
"      currentSuite.after = teardown\n"
"      return currentSuite\n"
"    end\n"
"  end\n"
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
"      currentTest = {test=test, suite=currentSuite, case=currentCase}\n"
"      \n"
"      cmpmet = {}\n"
"      cmpmet.report = function(self,okmsg,failmsg,success,should,result)\n"
"        if type(should) == \"table\" then should = table.concat(should, \", \") end\n"
"        if type(should) == \"function\" then should = \"\" end\n"
"        if self.invert then\n"
"          if success then\n"
"            return false, \"\"..table.concat(result,\", \")..okmsg..should..\"\"\n"
"          else\n"
"            return true, \"\"..table.concat(result,\", \")..failmsg..should..\"\"\n"
"          end\n"
"        else\n"
"          if success then\n"
"            return true, \"\"..table.concat(result,\", \")..okmsg..should..\"\"\n"
"          else\n"
"            return false, \"\"..table.concat(result,\", \")..failmsg..should..\"\"\n"
"          end\n"
"        end\n"
"      end\n"
"      \n"
"      cmpmet.equal = function(self,should)\n"
"        currentTest.try = function(result)\n"
"          same = true\n"
"          if type(should) == \"table\" and type(result) == \"table\" then\n"
"            for i,v in ipairs(should) do\n"
"              if v ~= result[i] then\n"
"                same = false\n"
"              end\n"
"            end\n"
"          elseif type(should) == \"string\" and type(result) == \"table\" then\n"
"            if table.getn(result) == 1 then\n"
"              same = should == result[1]\n"
"            else\n"
"              same = false\n"
"            end\n"
"          else\n"
"            return false, \"Comparison data needs to be tables: should is '\"..type(should)..\"', result is '\"..type(result)..\"'\"\n"
"          end\n"
"          return self:report(\" is equal to \",\" is not equal to \",same,should,result)\n"
"        end\n"
"        return currentCase\n"
"      end\n"
"      \n"
"      cmpmet.be_nil = function(self)\n"
"        currentTest.try = function(result)\n"
"          if type(result) == \"table\" then\n"
"            same = true\n"
"            for i,v in ipairs(result) do\n"
"              if result[i] ~= nil then\n"
"                same = false\n"
"              end\n"
"            end\n"
"            return self:report(\" is nil \",\" is not nil \",same,\"nil\",result)\n"
"          else\n"
"            return false, \"Comparison data needs to be table: result is '\"..type(result)..\"'\"\n"
"          end\n"
"        end\n"
"        return currentCase\n"
"      end\n"
"      \n"
"      cmpmet.match = function(self,match)\n"
"        currentTest.try = function(result)\n"
"          if type(match) == \"string\" and type(result) == \"table\" then\n"
"            same = false\n"
"            for i,v in ipairs(result) do\n"
"              if string.find(result[i],match) ~= nil then\n"
"                same = true\n"
"              end\n"
"            end\n"
"            return self:report(\" does match \",\" does not match \",same,match,result)\n"
"          else\n"
"            return false, \"Comparison data needs to be string and table: should is '\"..type(should)..\"', result is '\"..type(result)..\"'\"\n"
"          end\n"
"        end\n"
"        return currentCase\n"
"      end\n"
"      \n"
"      cmpmet.be_true = function(self,should)\n"
"        currentTest.try = function(result)\n"
"          if type(should) == \"function\" and type(result) == \"table\" then\n"
"            same = should(result)\n"
"            return self:report(\" is true \",\" is not true \",same,should,result)\n"
"          else\n"
"            return false, \"Comparison data needs to be function and table: should is '\"..type(should)..\"', result is '\"..type(result)..\"'\"\n"
"          end\n"
"        end\n"
"        return currentCase\n"
"      end\n"
"      \n"
"      currentTest.should = {invert=false}\n"
"      currentTest.should.nt = {invert=true}\n"
"      for k,v in pairs(cmpmet) do\n"
"        currentTest.should[k] = v\n"
"        currentTest.should.nt[k] = v\n"
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
"  if table.getn(pdtest.queue) == 0 then\n"
"    return false\n"
"  elseif table.getn(pdtest.queue[1].queue) == 0 then\n"
"    table.insert(pdtest.dones, table.remove(pdtest.queue,1))\n"
"  elseif table.getn(pdtest.queue[1].queue[1].queue) == 0 then\n"
"    table.insert(pdtest.queue[1].dones, table.remove(pdtest.queue[1].queue,1))\n"
"  else\n"
"    current = pdtest.queue[1].queue[1].queue[1]\n"
"    current.suite.before()\n"
"    current.case.before()\n"
"    if type(current.test) == \"function\" then\n"
"      current.name = \"function\"\n"
"      current.test()\n"
"    elseif type(current.test) == \"table\" then\n"
"      current.name = table.concat(current.test, \", \")\n"
"      pdtest.message(current.test)\n"
"    else\n"
"      pdtest.error(\"wrong test data type -- \"..type(current.test)..\" -- should have been function or table\")\n"
"    end\n"
"    current.suite.after()\n"
"    current.case.after()\n"
"    table.insert(pdtest.currents, current)\n"
"    table.insert(pdtest.queue[1].queue[1].dones, table.remove(pdtest.queue[1].queue[1].queue,1))\n"
"  end\n"
"  return true\n"
"end\n";

const char* pdtest_lua_yield = "\n"
"function pdtest_yield()\n"
"  if table.getn(pdtest.currents) > 0 and table.getn(pdtest.results) > 0 then\n"
"    current = table.remove(pdtest.currents,1)\n"
"    result = table.remove(pdtest.results,1)\n"
"    current.result = result\n"
"    current.success, current.detail = current.try(current.result)\n"
"    nametag = \"\"..current.suite.name..\" -> \"..current.case.name..\" < \"..current.name..\" > \"\n"
"    if current.success then\n"
"      pdtest.post(nametag)\n"
"      pdtest.post(\"-> OK\")\n"
"    else\n"
"      pdtest.post(nametag)\n"
"      pdtest.post(\"x> FAILED |> \"..current.detail)\n"
"    end\n"
"  elseif table.getn(pdtest.currents) == 0 and table.getn(pdtest.results) == 0 and table.getn(pdtest.queue) == 0 then\n"
"    return false\n"
"  end\n"
"  return true\n"
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


