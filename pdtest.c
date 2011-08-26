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
} t_pdtest;

/* Declarations */

/* pdtest init methods */
void pdtest_setup(void);
void *pdtest_new(void);
void pdtest_free(t_pdtest *x);

/* pdtest inlets methods */
void pdtest_suite(t_pdtest *x, t_symbol *s, int argc, t_atom *argv);
void pdtest_result(t_pdtest *x, t_symbol *s, int argc, t_atom *argv);

/* helper functions */
void pdtest_luasetup(t_pdtest *x);
static const char *pdtest_lua_reader(lua_State *lua, void *rr, size_t *size);
void pdtest_lua_loadsuite(t_pdtest *x, const char *filename);

/* Lua functions */
static int luafunc_pdtest_message(lua_State *lua);
static int luafunc_pdtest_error(lua_State *lua);

/* Lua source code */
const char* pdtest_lua_init;

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
    
    int i;
    for (i = 0; i < argc; i++) {
        char filepath[256];
        atom_string(argv+i, filepath, 256);
        const char* filestring = (const char*)filepath;
        post("pdtest: loading testfile %s", filestring);
        
        pdtest_lua_loadsuite(x, filestring);
    }
}

void pdtest_result(t_pdtest *x, t_symbol *s, int argc, t_atom *argv)
{
  if (argc < 1) {
      post("pdtest: result is empty"); return;
  }
  
  lua_newtable(x->lua);
  for (i = 0; i < argc; i++) {
      char result[256];
      atom_string(argv+i, result, 256);
      
      const char* resultS = (const char*)result;
      lua_pushstring(x->lua,resultS)
      
      
  }
  
  post("pdtest: posting results...");
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
    lua_setglobal(x->lua,"pdtest");
    
    lua_pushlightuserdata(x->lua, x);
    lua_setglobal(x->lua,"pdtest_userdata");
    
    luaL_dostring(x->lua, pdtest_lua_init);
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
      if (lua_pcall(x->lua, 0, LUA_MULTRET, 0)) {
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



/*************************************************
 *               Lua Source Code                 *
 *                                               *
 *************************************************/

const char* pdtest_lua_init = ""
  "pdtest.queue={}                                      "
  "pdtest.results={}"
  "pdtest.suite = function(suite)                        "
  "  currentSuite = {name=suite, queue={}, results={}}               "
  "  currentSuite.setup = function() end                 "
  "  currentSuite.teardown = function() end              "
  "                                                      "
  "  currentSuite.case = function(case)                  "
  "    currentCase = {name=case, queue={}, results={}}               "
  "    currentCase.setup = function() end                "
  "    currentCase.teardown = function() end             "
  "                                                      "
  "    currentCase.test = function(test)                 "
  "      currentTest = {test=test}                       "
  "                                                      "
  "      currentTest.should = function(should)           "
  "        currentTest.should = should                   "
  "                                                      "
  "        return currentCase                            "
  "      end                                             "
  "                                                      "
  "      table.insert(currentCase.queue,currentTest)     "
  "      return currentTest                              "
  "    end                                               "
  "                                                      "
  "    table.insert(currentSuite.queue,currentCase)      "
  "    return currentCase                                "
  "  end                                                 "
  "                                                      "
  "  table.insert(pdtest.results,currentSuite)            "
  "  return currentSuite                                 "
  "end                                                   "
  "pdtest.result = function(result)"
  "  "
  "end";

  