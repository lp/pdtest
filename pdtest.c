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
#define PDTEST_MINOR 8
#define PDTEST_PATCH 8
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
void *pdtest_new(t_symbol *s, int argc, t_atom *argv);
void pdtest_free(t_pdtest *x);

/* pdtest inlets methods */
void pdtest_suite(t_pdtest *x, t_symbol *s, int argc, t_atom *argv);
void pdtest_result(t_pdtest *x, t_symbol *s, int argc, t_atom *argv);
void pdtest_start(t_pdtest *x, t_symbol *s);
void pdtest_stop(t_pdtest *x, t_symbol *s);
void pdtest_reset(t_pdtest *x, t_symbol *s);
static int pdtest_continue(t_pdtest *x);

/* pdtest scheduler method */
static void pdtest_run(t_pdtest *x);
static void pdtest_next(t_pdtest * x);
static void pdtest_yield(t_pdtest * x);
static void pdtest_schedule(t_pdtest *x);

/* helper functions */
static void pdtest_luasetup(t_pdtest *x);
static const char *pdtest_lua_reader(lua_State *lua, void *rr, size_t *size);
static void pdtest_lua_loadsuite(t_pdtest *x, const char *filename);
static t_atom *pdtest_lua_popatomtable(lua_State *L, int *count);
static void pdtest_report(t_pdtest *x);
static int pdtest_is_rawmessage(t_pdtest *x);
static void pdtest_reg_message(t_pdtest *x, int israw);
static void pdtest_message(t_pdtest *x, int israw);

/* Lua functions */
static int luafunc_pdtest_test(lua_State *lua);
static int luafunc_pdtest_outlet(lua_State *lua);

static int luafunc_pdtest_error(lua_State *lua);
static int luafunc_pdtest_post(lua_State *lua);
static int luafunc_pdtest_errorHandler(lua_State *lua);
static int luafunc_pdtest_register(lua_State *lua);
static int luafunc_pdtest_unregister(lua_State *lua);

/* Lua source code */
static const char* pdtest_lua_init;
static const char* pdtest_lua_next;
static const char* pdtest_lua_yield;
static const char* pdtest_lua_report;
static const char* pdtest_lua_stdlib_set;
static const char* pdtest_lua_stdlib_math;

void pdtest_setup(void)
{
    pdtest_class = class_new(gensym("pdtest"),
        (t_newmethod)pdtest_new,
        (t_method)pdtest_free,
        sizeof(t_pdtest),
        CLASS_DEFAULT,
        A_GIMME,0);
    
    class_addmethod(pdtest_class,
        (t_method)pdtest_suite, gensym("suite"),
        A_GIMME, 0);
    class_addmethod(pdtest_class,
        (t_method)pdtest_result, gensym("result_list"),
        A_GIMME, 0);
    class_addmethod(pdtest_class,
        (t_method)pdtest_result, gensym("result_symbol"),
        A_GIMME, 0);
    class_addmethod(pdtest_class,
        (t_method)pdtest_result, gensym("result_float"),
        A_GIMME, 0);
    class_addmethod(pdtest_class,
        (t_method)pdtest_result, gensym("result_bang"),
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

void *pdtest_new(t_symbol *s, int argc, t_atom *argv)
{
  (void)s;
  t_pdtest *x = NULL;
  x = (t_pdtest*)pd_new(pdtest_class);
  
  x->canvas = canvas_getcurrent();
  pdtest_luasetup(x);
    
  x->async_run = 0;
  x->async_clock = clock_new(x, (t_method)pdtest_run);
  
  int i;
  for (i = 0; i < argc; i++) {
    t_symbol * sarg = atom_getsymbol(argv+i);
    if ((sarg == &s_list) || (sarg == gensym("l"))) {
      inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_list, gensym("result_list"));
    } else if ((sarg == &s_symbol) || (sarg == gensym("s"))) {
      inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_symbol, gensym("result_symbol"));
    } else if ((sarg == &s_float) || (sarg == gensym("f"))) {
      inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("result_float"));
    } else if ((sarg == &s_bang) || (sarg == gensym("b"))) {
      inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_bang, gensym("result_bang"));
    } else {
      char arg[16];
      atom_string(argv+i, arg, 16);
      error("pdtest: unknown inlet argument '%s'",(const char*)arg);
    }
  }
  
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
  if ((argc < 1) && (s != gensym("result_bang"))) {  /* skip result was empty ??? */
      error("pdtest: result message is empty"); return;}
  /* skip result if correspondent message was raw_message */
  if (pdtest_is_rawmessage(x)) { return; }
  
  /* gets _pdtest.results table onto the stack*/
  lua_getglobal(x->lua, "_pdtest");
  lua_getfield(x->lua, -1, "results");
  int n = luaL_getn(x->lua,-1);
  
  if (s == gensym("result_list")) {
    /* build result message table from argv */
    lua_newtable(x->lua);
    int i;
    for (i = 0; i < argc; i++) {
        char result[256];
        atom_string(argv+i, result, 256);
        const char* resultstring = (const char*)result;
        lua_pushstring(x->lua,resultstring);
        lua_rawseti(x->lua,-2,i+1);
    }
  } else if (s == gensym("result_symbol")) {
    char result[256];
    atom_string(argv, result, 256);
    const char* resultstring = (const char*)result;
    lua_pushstring(x->lua,resultstring);
  } else if   (s == gensym("result_float")) {
    double result = (double)atom_getfloatarg(0,argc,argv);
    lua_pushnumber(x->lua, result);
  } else if   (s == gensym("result_bang")) {
    const char* resultstring = "bang";
    lua_pushstring(x->lua, resultstring);
  }
  
  lua_rawseti(x->lua,-2,n+1);   /* push result message in _pdtest.table */
  lua_pop(x->lua,2);            /* clean up the stack */
  pdtest_start(x,gensym("result"));
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
    lua_getglobal(x->lua,"_pdtest");
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
    /* prepares the stack to call _pdtest.next() */
    lua_getglobal(x->lua,"_pdtest");
    lua_getfield(x->lua, -1, "errorHandler");
    lua_getfield(x->lua, -2, "next");
    lua_remove(x->lua,-3);
    if (!lua_isfunction(x->lua,-1)) {
      lua_pop(x->lua,2);
      error("pdtest: no pdtest_next function!!!");
      return;
    }
    lua_pcall(x->lua, 0, 1, 0);
    
    if (lua_isfunction(x->lua,-2))    /* clean error handler from stack */
      lua_remove(x->lua,-2);
}

static void pdtest_yield(t_pdtest * x)
{
    /* prepares the stack to call _pdtest.yield() */
    lua_getglobal(x->lua,"_pdtest");
    lua_getfield(x->lua, -1, "errorHandler");
    lua_getfield(x->lua, -2, "yield");
    lua_remove(x->lua,-3);
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
            int report = lua_toboolean(x->lua,-1);
            if (report) {
                pdtest_report(x);
            }
        } else { error("pdtest: _pdtest.yield() didn't return a bool??"); }
        lua_pop(x->lua,1);  /* clean stack from bool result */
    }
}

static void pdtest_schedule(t_pdtest *x)
{
    if (x->async_run && pdtest_continue(x)) {
        clock_delay(x->async_clock, 0);
    } else {
        x->async_run = 0;
        clock_unset(x->async_clock);
    }
}

static int pdtest_continue(t_pdtest *x)
{
  lua_getglobal(x->lua,"_pdtest");
  
  lua_getfield(x->lua, -1, "queue");
  int queue_n = luaL_getn(x->lua,-1);
  
  lua_getfield(x->lua, -2, "results");
  int results_n = luaL_getn(x->lua,-1);
  
  lua_getfield(x->lua, -3, "currents");
  int currents_n = luaL_getn(x->lua,-1);
  
  lua_pop(x->lua,4);
  if ((queue_n > 0) ||
    ((results_n > 0) && (currents_n > 0))) {
    return 1;
  } else {
    return 0;
  }
}

/*************************************************
 *               Helper functions                *
 *                                               *
 *************************************************/

static void pdtest_luasetup(t_pdtest *x)
{
    /* gets new lua state and loads standard libs */
    x->lua = lua_open();
    luaL_openlibs(x->lua);
    
    /* prepare global _ namespace */
    lua_newtable(x->lua);
    lua_pushcfunction(x->lua, luafunc_pdtest_error);
    lua_setfield(x->lua, -2, "error");
    lua_pushcfunction(x->lua, luafunc_pdtest_post);
    lua_setfield(x->lua, -2, "post");
    lua_pushcfunction(x->lua, luafunc_pdtest_outlet);
    lua_setfield(x->lua, -2, "outlet");
    lua_setglobal(x->lua,"_");
    
    /* prepare global _pdtest namespace */
    lua_newtable(x->lua);
    lua_pushcfunction(x->lua, luafunc_pdtest_test);
    lua_setfield(x->lua, -2, "test");
    lua_pushcfunction(x->lua, luafunc_pdtest_register);
    lua_setfield(x->lua, -2, "register");
    lua_pushcfunction(x->lua, luafunc_pdtest_unregister);
    lua_setfield(x->lua, -2, "unregister");
    lua_pushcfunction(x->lua, luafunc_pdtest_errorHandler);
    lua_setfield(x->lua, -2, "errorHandler");
    lua_pushlightuserdata(x->lua, x);
    lua_setfield(x->lua, -2, "userdata");
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
    lua_setglobal(x->lua,"_pdtest");
    
    /* execute lua source code embeded in C strings to initialize lua test system */
    int err;
    err = luaL_dostring(x->lua, pdtest_lua_stdlib_set);
    if (err) {
        if (lua_isstring(x->lua,-1))
            error("error loading 'pdtest_lua_stdlib_set': %s", lua_tostring(x->lua,-1));
    }
    err = luaL_dostring(x->lua, pdtest_lua_stdlib_math);
    if (err) {
        if (lua_isstring(x->lua,-1))
            error("error loading 'pdtest_lua_stdlib_math': %s", lua_tostring(x->lua,-1));
    }
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
static void pdtest_lua_loadsuite(t_pdtest *x, const char *filename)
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
    error("pdtest: Test.list() function takes a table as its only argument");
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
    /* prepares the stack to call _pdtest.report() */
    lua_getglobal(x->lua,"_pdtest");
    lua_getfield(x->lua, -1, "errorHandler");
    lua_getfield(x->lua, -2, "report");
    lua_remove(x->lua,-3);
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
                pdtest_start(x,gensym("report"));
            }
        } else { error("pdtest: _pdtest.report() didn't return a bool??"); }
        lua_pop(x->lua,1);  /* clean stack from bool result */
    }
}

static int pdtest_is_rawmessage(t_pdtest *x)
{
    /* prepares the stack to call _pdtest.unregister() */
    lua_getglobal(x->lua,"_pdtest");
    lua_getfield(x->lua, -1, "errorHandler");
    lua_getfield(x->lua, -2, "unregister");
    lua_remove(x->lua,-3);
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
    /* prepares the stack to call _pdtest.register() */
    lua_getglobal(x->lua,"_pdtest");
    lua_getfield(x->lua, -1, "errorHandler");
    lua_getfield(x->lua, -2, "register");
    lua_remove(x->lua,-3);
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

static void pdtest_message(t_pdtest *x, int israw)
{
    if (lua_istable(x->lua,-1)) {
      int count = 0;
      t_atom *message = pdtest_lua_popatomtable(x->lua,&count);
      pdtest_reg_message(x,israw);  /* register message as test mesage */
      outlet_list(x->x_obj.ob_outlet, &s_list, count, &message[0]);   /* sends list message to outlet */
      
    } else if (lua_isnumber(x->lua,-1)) {
      double message = lua_tonumber(x->lua,-1);
      lua_pop(x->lua,1);
      pdtest_reg_message(x,israw);  /* register message as test mesage */
      outlet_float(x->x_obj.ob_outlet, (t_float)message); /* sends float message to outlet */
    } else if (lua_isstring(x->lua,-1)) {
      const char* message = lua_tostring(x->lua,-1);
      lua_pop(x->lua,1);
      pdtest_reg_message(x,israw);  /* register message as test mesage */
      
      const char* bang = "bang";
      if (strcmp(message,bang) == 0) {
        outlet_bang(x->x_obj.ob_outlet); /* sends bang message to outlet */
      } else {
        outlet_symbol(x->x_obj.ob_outlet, gensym(message)); /* sends symbol message to outlet */
      }
    } else {
      error("pdtest: pdtest_message function needs a valid msgtype");
    }
}

/*************************************************
 *               Lua functions                   *
 *                                               *
 *************************************************/

static int luafunc_pdtest_test(lua_State *lua)
{
  lua_getglobal(lua,"_pdtest");
  lua_getfield(lua, -1, "userdata"); /* get t_pdtest since we're in lua now */
  lua_remove(lua,-2);
  if (lua_islightuserdata(lua,-1)) {
    t_pdtest *x = lua_touserdata(lua,-1);
    lua_pop(lua,1);           /* clean up stack of userdata */
    pdtest_message(x, 0);
  } else {
    lua_pop(lua,1); /* cleaning stack of unknown object */
    error("pdtest: userdata missing from Lua stack");
  }
  return 0;
}

static int luafunc_pdtest_outlet(lua_State *lua)
{
  lua_getglobal(lua,"_pdtest");
  lua_getfield(lua, -1, "userdata"); /* get t_pdtest since we're in lua now */
  lua_remove(lua,-2);
  if (lua_islightuserdata(lua,-1)) {
    t_pdtest *x = lua_touserdata(lua,-1);
    lua_pop(lua,1);           /* clean up stack of userdata */
    pdtest_message(x, 1);
  } else {
    lua_pop(lua,1); /* cleaning stack of unknown object */
    error("pdtest: userdata missing from Lua stack");
  }
  return 0;
}

/* lua function - sends lua string as error to pd console */
static int luafunc_pdtest_error(lua_State *lua)
{
  const char *s = luaL_checkstring(lua, 1);
  if (lua_isstring(lua,-1))
    lua_pop(lua,1);   /* clean up stack of error string */
  if (s) {
    error("pdtest: %s", s);
  } else {
    error("pdtest: error in posting pd error, no error message string in Lua stack");
  }
  return 0;
}

/* lua function - sends lua string as post to pd console */
static int luafunc_pdtest_post(lua_State *lua)
{
  const char *s = luaL_checkstring(lua, 1);
  if (lua_isstring(lua,-1))
    lua_pop(lua,1);   /* clean up stack of post string */
  if (s) {
    post("pdtest: %s", s);
  } else {
    error("pdtest: error in posting pd post, no message string in Lua stack");
  }
  return 0;
}

/* lua function used in C - error handler callback */
static int luafunc_pdtest_errorHandler(lua_State *lua)
{
    const char* err;
    if (lua_isstring(lua,-1)) {
        err = lua_tostring(lua,-1);
    } else {
        err = " ";
    }
    lua_pop(lua,1); /* cleaning stack of error string */
    error("pdtest: lua_pcall error: %s", err);
    return 0;
}

/* lua function used in C - register message as test w/0 or raw w/1 */
static int luafunc_pdtest_register(lua_State *lua)
{
    if (!lua_isboolean(lua,-1)) {
      lua_pop(lua,-1);
      error("pdtest: no boolean as _pdtest.register() argument");
      return 0;
    }
    lua_getglobal(lua,"_pdtest");
    lua_getfield(lua, -1, "reg");
    if (!lua_istable(lua,-1)) {
        lua_pop(lua,2);
        error("pdtest: no _pdtest.reg table!!!");
        return 0;
    }
    lua_remove(lua,-2);   /* clean stack of pdtest table */
    
    int n = luaL_getn(lua,-1);
    lua_insert(lua,-2);   /* insert reg behind bool */
    lua_rawseti(lua,-2,n+1);
    lua_pop(lua,1);       /* clean stack of reg table */
    return 0;
}

/* lua function - unregister next message and return its bool */
static int luafunc_pdtest_unregister(lua_State *lua)
{
    lua_getglobal(lua,"_pdtest");
    lua_getfield(lua, -1, "reg");
    if (!lua_istable(lua,-1)) {
      lua_pop(lua,2);
      error("pdtest: no _pdtest.reg table!!!");
      lua_pushnil(lua);
      return 1;
    }
    lua_remove(lua,-2);   /* clean stack of pdtest table */
    
    int n = luaL_getn(lua,-1);
    if (n == 0) {
      lua_pop(lua,-1);    /* clean stack of empty reg */
      error("pdtest: unregistering on an empty queue?");
      lua_pushnil(lua);
      return 1;
    }
    luaL_setn(lua, -1, n-1);  /* sets new reg table size */
    
    int pos = 1;
    /* place the first message status bool behind the reg table */
    /* leave it there to be harvested when function returns */
    lua_rawgeti(lua,-1,pos);
    lua_insert(lua,-2);   
    
    /* ripple down indexes of other message in the reg */
    for ( ;pos<n;pos++) {
        lua_rawgeti(lua, -1, pos+1);
        lua_rawseti(lua, -2, pos);
    }
    lua_pushnil(lua);
    lua_rawseti(lua, -2, n);
    
    lua_pop(lua,1);   /* clean stack of reg table */
    return 1;
}

/*************************************************
 *               Lua Source Code                 *
 *                                               *
 *************************************************/

static const char* pdtest_lua_init = "\n"
"Suite = function(suite)\n"
"  local currentSuite = {name=suite, queue={}, dones={}}\n"
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
"    local currentCase = {name=case, queue={}, dones={}}\n"
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
"      local currentTest = {test=test, suite=currentSuite, case=currentCase}\n"
"      \n"
"      local cmpmet = {}\n"
"      cmpmet.report = function(self,okmsg,failmsg,success,should,result)\n"
"        if type(should) == \"table\" then\n"
"          if type(should[1]) == \"table\" then\n"
"            local nshould = {}\n"
"            for i,v in ipairs(should) do\n"
"              if type(v) == \"table\" then\n"
"                nshould[i] = table.concat(v,\", \")\n"
"              else\n"
"                nshould[i] = \"nil\"\n"
"              end\n"
"            end\n"
"            should = table.concat(nshould, \" | \")\n"
"          else\n"
"            should = table.concat(should, \", \")\n"
"          end\n"
"        elseif type(should) == \"number\" then\n"
"          should = tostring(should)\n"
"        elseif type(should) == \"function\" then\n"
"          should = \"function\"\n"
"        end\n"
"        \n"
"        if type(result) == \"table\" then\n"
"          result = table.concat(result,\", \")\n"
"        elseif type(result) == \"number\" then\n"
"          result = tostring(result)\n"
"        end\n"
"        \n"
"        if self.invert then\n"
"          if success then\n"
"            return false, \"\"..result..okmsg..should..\"\"\n"
"          else\n"
"            return true, \"\"..result..failmsg..should..\"\"\n"
"          end\n"
"        else\n"
"          if success then\n"
"            return true, \"\"..result..okmsg..should..\"\"\n"
"          else\n"
"            return false, \"\"..result..failmsg..should..\"\"\n"
"          end\n"
"        end\n"
"      end\n"
"      \n"
"      cmpmet.numbers = function(self,should,result)\n"
"        if type(should) == \"number\" and type(result) == \"number\" then\n"
"          return should, result\n"
"        elseif (type(should) == \"number\" or type(should) == \"string\") and\n"
"          (type(result) == \"number\" or type(result) == \"string\") then\n"
"          return tonumber(should), tonumber(result)\n"
"        else\n"
"          local tshould = nil\n"
"          local tresult = nil\n"
"          if type(should) == \"table\" then\n"
"            if type(should[1]) == \"number\" then\n"
"              tshould = should[1]\n"
"            elseif type(should[1]) == \"string\" then\n"
"              tshould = tonumber(should[1])\n"
"            end\n"
"          elseif type(should) == \"string\" then\n"
"            tshould = tonumber(should)\n"
"          elseif type(should) == \"number\" then\n"
"            tshould = should\n"
"          end\n"
"\n"
"          if type(result) == \"table\" then\n"
"            if type(result[1]) == \"number\" then\n"
"              tresult = result[1]\n"
"            elseif type(result[1]) == \"string\" then\n"
"              tresult = tonumber(result[1])\n"
"            end\n"
"          elseif type(result) == \"string\" then\n"
"            tresult = tonumber(result)\n"
"          elseif type(result) == \"number\" then\n"
"            tresult = result\n"
"          end\n"
"          return tshould, tresult\n"
"        end\n"
"      end\n"
"      \n"
"      cmpmet.equal = function(self,should)\n"
"        currentTest.try = function(result)\n"
"          local same = true\n"
"          if type(should) == \"table\" and type(result) == \"table\" then\n"
"            for i,v in ipairs(should) do\n"
"              if v ~= result[i] then\n"
"                same = false\n"
"              end\n"
"            end\n"
"          elseif (type(should) == \"string\" and type(result) == \"string\") or\n"
"            type(should) == \"number\" and type(result) == \"number\" then\n"
"            same = should == result\n"
"          else\n"
"            return false, \"Comparison data needs to be of similar type: should is '\"..type(should)..\"', result is '\"..type(result)..\"'\"\n"
"          end\n"
"          return self:report(\" is equal to \",\" is not equal to \",same,should,result)\n"
"        end\n"
"        return currentCase\n"
"      end\n"
"      \n"
"      cmpmet.resemble = function(self,should,p)\n"
"        currentTest.try = function(result)\n"
"          local same = true\n"
"          if type(should) == \"table\" and type(result) == \"table\" then\n"
"            should_set = set.new(should)\n"
"            result_set = set.new(result)\n"
"            same = set.equal(should_set,result_set)\n"
"          elseif type(should) == \"string\" and type(result) == \"string\" then\n"
"            same = should:lower() == result:lower()\n"
"          elseif type(should) == \"number\" and type(result) == \"number\" then\n"
"            local precision = 0\n"
"            if type(p) == \"number\" then\n"
"              precision = p\n"
"            end\n"
"            same = math.round(should,precision) == math.round(result,precision)\n"
"          elseif type(should) == type(result) then\n"
"            same = should == result\n"
"          elseif (type(should) == \"string\" and type(result) == \"number\") or\n"
"            (type(result) == \"string\" and type(should) == \"number\") then\n"
"            if type(p) == \"number\" then\n"
"              same = math.round(tonumber(should),p) == math.round(tonumber(result),p)\n"
"            else\n"
"              same = tonumber(should) == tonumber(result)\n"
"            end\n"
"          else\n"
"            if type(should) == \"table\" then\n"
"              if type(should[1]) == \"number\" or type(result) == \"number\" then\n"
"                if type(p) == \"number\" then\n"
"                  same = math.round(tonumber(should[1]),p) == math.round(tonumber(result),p)\n"
"                else\n"
"                  same = tonumber(should[1]) == tonumber(result)\n"
"                end\n"
"              else\n"
"                same = tostring(should[1]):lower() == tostring(result):lower()\n"
"              end\n"
"            elseif type(result) == \"table\" then\n"
"              if type(result[1]) == \"number\" or type(should) == \"number\" then\n"
"                if type(p) == \"number\" then\n"
"                  same = math.round(tonumber(result[1]),p) == math.round(tonumber(should),p)\n"
"                else\n"
"                  same = tonumber(result[1]) == tonumber(should)\n"
"                end\n"
"              else\n"
"                same = tostring(result[1]):lower() == tostring(should):lower()\n"
"              end\n"
"            end\n"
"          end\n"
"          return self:report(\" does resemble \",\" does not resemble \",same,should,result)\n"
"        end\n"
"      end\n"
"      \n"
"      cmpmet.be_more = function(self,should)\n"
"        currentTest.try = function(result)\n"
"          local tshould, tresult = self:numbers(should,result)\n"
"          if type(tshould) ~= \"nil\" and type(tresult) ~= \"nil\" then\n"
"            local more =  tresult > tshould\n"
"            return self:report(\" is more than \",\" is not more than \", more,should,result)\n"
"          else\n"
"            return false, \"Comparison data needs to be tables, numbers or strings: should is '\"..type(should)..\"', result is '\"..type(result)..\"'\"\n"
"          end\n"
"        end\n"
"        return currentCase\n"
"      end\n"
"      \n"
"      cmpmet.be_less = function(self,should)\n"
"        currentTest.try = function(result)\n"
"          local tshould, tresult = self:numbers(should,result)\n"
"          if type(tshould) ~= \"nil\" and type(tresult) ~= \"nil\" then\n"
"            local less =  tresult < tshould\n"
"            return self:report(\" is less than \",\" is not less than \", less,should,result)\n"
"          else\n"
"            return false, \"Comparison data needs to be tables, numbers or strings: should is '\"..type(should)..\"', result is '\"..type(result)..\"'\"\n"
"          end\n"
"        end\n"
"        return currentCase\n"
"      end\n"
"      \n"
"      cmpmet.be_more_or_equal = function(self,should)\n"
"        currentTest.try = function(result)\n"
"          local tshould, tresult = self:numbers(should,result)\n"
"          if type(tshould) ~= \"nil\" and type(tresult) ~= \"nil\" then\n"
"            local more =  tresult >= tshould\n"
"            return self:report(\" is more or equal than \",\" is not more or equal than \", more,should,result)\n"
"          else\n"
"            return false, \"Comparison data needs to be tables, numbers or strings: should is '\"..type(should)..\"', result is '\"..type(result)..\"'\"\n"
"          end\n"
"        end\n"
"        return currentCase\n"
"      end\n"
"      \n"
"      cmpmet.be_less_or_equal = function(self,should)\n"
"        currentTest.try = function(result)\n"
"          local tshould, tresult = self:numbers(should,result)\n"
"          if type(tshould) ~= \"nil\" and type(tresult) ~= \"nil\" then\n"
"            local less =  tresult <= tshould\n"
"            return self:report(\" is less or equal than \",\" is not less or equal than \", less,should,result)\n"
"          else\n"
"            return false, \"Comparison data needs to be tables, numbers or strings: should is '\"..type(should)..\"', result is '\"..type(result)..\"'\"\n"
"          end\n"
"        end\n"
"        return currentCase\n"
"      end\n"
"      \n"
"      cmpmet.be_nil = function(self)\n"
"        currentTest.try = function(result)\n"
"          local same = true\n"
"          if type(result) == \"table\" then\n"
"            for i,v in ipairs(result) do\n"
"              if result[i] ~= nil then\n"
"                same = false\n"
"              end\n"
"            end\n"
"          elseif type(result) == \"string\" or type(result) == \"number\" then\n"
"            same = false\n"
"          elseif type(result) == nil then\n"
"            same = true\n"
"          else\n"
"            return false, \"Comparison result needs to be table, string or number: result is '\"..type(result)..\"'\"\n"
"          end\n"
"          \n"
"          return self:report(\" is nil \",\" is not nil \",same,\"nil\",result)\n"
"        end\n"
"        return currentCase\n"
"      end\n"
"      \n"
"      cmpmet.match = function(self,match)\n"
"        currentTest.try = function(result)\n"
"          if type(match) == \"string\" then\n"
"            local same = false\n"
"            if type(result) == \"table\" then\n"
"              for i,v in ipairs(result) do\n"
"                if string.find(result[i],match) ~= nil then\n"
"                  same = true\n"
"                end\n"
"              end\n"
"            elseif type(result) == \"string\" then\n"
"              if string.find(result,match) ~= nil then\n"
"                same = true\n"
"              end\n"
"            elseif type(result) == \"number\" then\n"
"              if string.find(tostring(result),match) ~= nil then\n"
"                same = true\n"
"              end\n"
"            else\n"
"              return false, \"Comparison result needs to be list,string or number: result is '\"..type(result)..\"'\"\n"
"            end\n"
"            \n"
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
"          if type(should) == \"function\" then\n"
"            local same = should(result)\n"
"            return self:report(\" is true \",\" is not true \",same,should,result)\n"
"          else\n"
"            return false, \"Comparison data needs to be function: should is '\"..type(should)..\"'\"\n"
"          end\n"
"        end\n"
"        return currentCase\n"
"      end\n"
"      \n"
"      cmpmet.be_any = function(self,should)\n"
"        currentTest.try = function(result)\n"
"          local same = true\n"
"          if type(should) == \"table\"  and\n"
"            #should > 0 and type(should[1]) == \"table\" and\n"
"            #should[1] > 0 and type(result) == \"table\" then\n"
"            for i,v in ipairs(should) do\n"
"              for si,sv in ipairs(v) do\n"
"                if sv ~= result[si] then\n"
"                  same = false\n"
"                end\n"
"              end\n"
"              if same then\n"
"                break\n"
"              elseif i < #should then\n"
"                same = true\n"
"              end\n"
"            end\n"
"          elseif type(should) == \"table\"  and #should > 0 and type(should[1]) == \"string\" then\n"
"            local result_value = \"\"\n"
"            if type(result) == \"string\" then\n"
"              result_value = result\n"
"            elseif type(result) == \"number\" then\n"
"              result_value = tostring(result)\n"
"            elseif type(result) == \"table\" and #result == 1 then\n"
"              result_value = tostring(result[1])\n"
"            else\n"
"              return false, \"Comparison result table needs to have only one member to be comparable with 'be_any': result is '\"..type(result)..\"' of length \"..tostring(#result)..\"\"\n"
"            end\n"
"            local should_set = set.new(should)\n"
"            same = set.member(should_set,result_value)\n"
"          elseif type(should) == \"table\"  and #should > 0 and type(should[1]) == \"number\" then\n"
"            local result_value = 0\n"
"            if type(result) == \"string\" then\n"
"              result_value = tonumber(result)\n"
"            elseif type(result) == \"number\" then\n"
"              result_value = result\n"
"            elseif type(result) == \"table\" and #result == 1 then\n"
"              result_value = tonumber(result[1])\n"
"            else\n"
"              return false, \"Comparison result table needs to have only one member to be comparable with 'be_any': result is '\"..type(result)..\"' of length \"..tostring(#result)..\"\"\n"
"            end\n"
"            local should_set = set.new(should)\n"
"            same = set.member(should_set,result_value)\n"
"          else\n"
"            return false, \"Comparison data needs to be table: should is '\"..type(should)..\"'\"\n"
"          end\n"
"          return self:report(\" is any \",\" is not any one \",same,should,result)\n"
"        end\n"
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
"  table.insert(_pdtest.queue,currentSuite)\n"
"  return currentSuite\n"
"end\n";

static const char* pdtest_lua_next = "\n"
"_pdtest.next = function()\n"
"  if table.getn(_pdtest.queue[1].queue) == 0 then\n"
"    table.insert(_pdtest.dones, table.remove(_pdtest.queue,1))\n"
"  elseif table.getn(_pdtest.queue[1].queue[1].queue) == 0 then\n"
"    table.insert(_pdtest.queue[1].dones, table.remove(_pdtest.queue[1].queue,1))\n"
"  else\n"
"    local current = _pdtest.queue[1].queue[1].queue[1]\n"
"    current.suite.before()\n"
"    current.case.before()\n"
"    if type(current.test) == \"function\" then\n"
"      current.name = \"function\"\n"
"      current.test(_pdtest.test)\n"
"    elseif type(current.test) == \"table\" then\n"
"      current.name = table.concat(current.test, \", \")\n"
"      _pdtest.test(current.test)\n"
"    elseif type(current.test) == \"string\" then\n"
"      current.name = current.test\n"
"      _pdtest.test(current.test)\n"
"    elseif type(current.test) == \"number\" then\n"
"      current.name = tostring(current.test)\n"
"      _pdtest.test(current.test)\n"
"    else\n"
"      _.error(\"wrong test data type -- \"..type(current.test)..\" -- should have been function or table\")\n"
"    end\n"
"    current.suite.after()\n"
"    current.case.after()\n"
"    table.insert(_pdtest.currents, current)\n"
"    table.insert(_pdtest.queue[1].queue[1].dones, table.remove(_pdtest.queue[1].queue[1].queue,1))\n"
"  end\n"
"end\n";

static const char* pdtest_lua_yield = "\n"
"_pdtest.yield = function()\n"
"  if table.getn(_pdtest.currents) > 0 and table.getn(_pdtest.results) > 0 then\n"
"    local current = table.remove(_pdtest.currents,1)\n"
"    local result = table.remove(_pdtest.results,1)\n"
"    current.result = result\n"
"    current.success, current.detail = current.try(current.result)\n"
"    local nametag = \"\"..current.suite.name..\" -> \"..current.case.name..\" >> \"..current.detail\n"
"    if current.success then\n"
"      _.post(nametag)\n"
"      _.post(\"-> OK\")\n"
"    else\n"
"      _.post(nametag)\n"
"      _.post(\"x> FAILED\")\n"
"    end\n"
"  end\n"
"  \n"
"  if table.getn(_pdtest.currents) == 0 and table.getn(_pdtest.results) == 0 and table.getn(_pdtest.queue) == 0 then\n"
"    return true\n"
"  end\n"
"  return false\n"
"end\n";

static const char* pdtest_lua_report = "\n"
"_pdtest.report = function()\n"
"  if table.getn(_pdtest.currents) > 0 then\n"
"    return false\n"
"  else\n"
"    _.post(\"!!! Test Completed !!!\")\n"
"    local tests = 0\n"
"    local cases = 0\n"
"    local suites = 0\n"
"    local ok = 0\n"
"    local fail = 0\n"
"    for si, suite in ipairs(_pdtest.dones) do\n"
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
"    _.post(\"\"..suites..\" Suites | \"..cases..\" Cases | \"..tests..\" Tests\")\n"
"    _.post(\"\"..ok..\" tests passed\")\n"
"    _.post(\"\"..fail..\" tests failed\")\n"
"  end\n"
"  return true\n"
"end\n";

static const char* pdtest_lua_stdlib_set = "\n"
"-- Set datatype.\n"
"module (\"set\", package.seeall)\n"
"\n"
"\n"
"-- Primitive methods (know about representation)\n"
"-- The representation is a table whose tags are the elements, and\n"
"-- whose values are true.\n"
"\n"
"--- Say whether an element is in a set\n"
"-- @param s set\n"
"-- @param e element\n"
"-- @return <code>true</code> if e is in set, <code>false</code>\n"
"-- otherwise\n"
"function member (s, e)\n"
"  return rawget (s, e) == true\n"
"end\n"
"\n"
"--- Insert an element into a set\n"
"-- @param s set\n"
"-- @param e element\n"
"function insert (s, e)\n"
"  rawset (s, e, true)\n"
"end\n"
"\n"
"--- Delete an element from a set\n"
"-- @param s set\n"
"-- @param e element\n"
"function delete (s, e)\n"
"  rawset (s, e, nil)\n"
"end\n"
"\n"
"--- Make a list into a set\n"
"-- @param l list\n"
"-- @return set\n"
"local metatable = {}\n"
"function new (l)\n"
"  local s = setmetatable ({}, metatable)\n"
"  for _, e in ipairs (l) do\n"
"    insert (s, e)\n"
"  end\n"
"  return s\n"
"end\n"
"\n"
"--- Iterator for sets\n"
"-- TODO: Make the iterator return only the key\n"
"elements = pairs\n"
"\n"
"\n"
"-- High level methods (representation-independent)\n"
"\n"
"--- Find the difference of two sets\n"
"-- @param s set\n"
"-- @param t set\n"
"-- @return s with elements of t removed\n"
"function difference (s, t)\n"
"  local r = new {}\n"
"  for e in elements (s) do\n"
"    if not member (t, e) then\n"
"      insert (r, e)\n"
"    end\n"
"  end\n"
"  return r\n"
"end\n"
"\n"
"--- Find the symmetric difference of two sets\n"
"-- @param s set\n"
"-- @param t set\n"
"-- @return elements of s and t that are in s or t but not both\n"
"function symmetric_difference (s, t)\n"
"  return difference (union (s, t), intersection (t, s))\n"
"end\n"
"\n"
"--- Find the intersection of two sets\n"
"-- @param s set\n"
"-- @param t set\n"
"-- @return set intersection of s and t\n"
"function intersection (s, t)\n"
"  local r = new {}\n"
"  for e in elements (s) do\n"
"    if member (t, e) then\n"
"      insert (r, e)\n"
"    end\n"
"  end\n"
"  return r\n"
"end\n"
"\n"
"--- Find the union of two sets\n"
"-- @param s set\n"
"-- @param t set\n"
"-- @return set union of s and t\n"
"function union (s, t)\n"
"  local r = new {}\n"
"  for e in elements (s) do\n"
"    insert (r, e)\n"
"  end\n"
"  for e in elements (t) do\n"
"    insert (r, e)\n"
"  end\n"
"  return r\n"
"end\n"
"\n"
"--- Find whether one set is a subset of another\n"
"-- @param s set\n"
"-- @param t set\n"
"-- @return <code>true</code> if s is a subset of t, <code>false</code>\n"
"-- otherwise\n"
"function subset (s, t)\n"
"  for e in elements (s) do\n"
"    if not member (t, e) then\n"
"      return false\n"
"    end\n"
"  end\n"
"  return true\n"
"end\n"
"\n"
"--- Find whether one set is a proper subset of another\n"
"-- @param s set\n"
"-- @param t set\n"
"-- @return <code>true</code> if s is a proper subset of t, false otherwise\n"
"function propersubset (s, t)\n"
"  return subset (s, t) and not subset (t, s)\n"
"end\n"
"\n"
"--- Find whether two sets are equal\n"
"-- @param s set\n"
"-- @param t set\n"
"-- @return <code>true</code> if sets are equal, <code>false</code>\n"
"-- otherwise\n"
"function equal (s, t)\n"
"  return subset (s, t) and subset (t, s)\n"
"end\n"
"\n"
"--- Metamethods for sets\n"
"-- set:method ()\n"
"metatable.__index = _M\n"
"-- set + table = union\n"
"metatable.__add = union\n"
"-- set - table = set difference\n"
"metatable.__sub = difference\n"
"-- set * table = intersection\n"
"metatable.__mul = intersection\n"
"-- set / table = symmetric difference\n"
"metatable.__div = symmetric_difference\n"
"-- set <= table = subset\n"
"metatable.__le = subset\n"
"-- set < table = proper subset\n"
"metatable.__lt = propersubset\n";

static const char* pdtest_lua_stdlib_math = "\n"
"--- Additions to the math module.\n"
"module (\"math\", package.seeall)\n"
"\n"
"\n"
"local _floor = floor\n"
"\n"
"--- Extend <code>math.floor</code> to take the number of decimal places.\n"
"-- @param n number\n"
"-- @param p number of decimal places to truncate to (default: 0)\n"
"-- @return <code>n</code> truncated to <code>p</code> decimal places\n"
"function floor (n, p)\n"
"  if p and p ~= 0 then\n"
"    local e = 10 ^ p\n"
"    return _floor (n * e) / e\n"
"  else\n"
"    return _floor (n)\n"
"  end\n"
"end\n"
"\n"
"--- Round a number to a given number of decimal places\n"
"-- @param n number\n"
"-- @param p number of decimal places to round to (default: 0)\n"
"-- @return <code>n</code> rounded to <code>p</code> decimal places\n"
"function round (n, p)\n"
"  local e = 10 ^ (p or 0)\n"
"  return _floor (n * e + 0.5) / e\n"
"end\n";

