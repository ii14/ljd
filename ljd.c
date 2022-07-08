#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <luajit.h>
#if LUAJIT_VERSION_NUM < 20100
#include "lj_frame.h"
#endif

#include "bp.h"


#define _CONCAT(a, b) a##b
#define CONCAT(a, b) _CONCAT(a, b)

#ifndef LUA_OK
# define LUA_OK 0
#endif
#if LUAJIT_VERSION_NUM < 20100
# define CAN_YIELD(L) (cframe_canyield((L)->cframe))
#else
# define CAN_YIELD(L) (lua_isyieldable(L))
#endif

#define LJD_DEBUGGER "ljd.debugger" // debugger metatable
#define LJD_RUNNING  "ljd.running"  // running debugger ref


typedef int LuaRef;


/// locals list
typedef struct {
  char *data;
  size_t len;
  size_t cap;
} locals_t;

/// init locals list
#define LOCALS_INIT (locals_t){ \
  .data = NULL, \
  .len = 0, \
  .cap = 0, \
}

static void locals_free(locals_t *locals)
{
  if (locals == NULL)
    return;
  free(locals->data);
  *locals = LOCALS_INIT;
}

static void locals_clear(locals_t *locals)
{
  if (locals == NULL)
    return;
  locals->len = 0;
}

static bool locals_push(locals_t *locals, const char *name)
{
  if (locals == NULL || name == NULL)
    return true;
  const size_t len = strlen(name);
  const size_t rsize = locals->len + len + 1;
  if (rsize > locals->cap) {
    const size_t ncap = locals->cap == 0 ? 1024 : locals->cap * 2;
    char* ndata = realloc(locals->data, ncap);
    if (ndata == NULL)
      return false;
    locals->data = ndata;
    locals->cap = ncap;
  }
  memcpy(locals->data + locals->len, name, len);
  locals->len += len;
  locals->data[locals->len++] = '\0';
  return true;
}


/// debugger object
typedef struct {
  LuaRef coro; /// debugged coroutine
  LuaRef func; /// debugged function

  int32_t currentbp; /// current breakpoint id
  int currentline; /// current line number
  char *currentsrc; /// current source

  int skipline; /// skip line
  int skiplevel; /// skip instructions above this call stack level
  bool continuing; /// is continue

  locals_t locals; /// local variables
  bps_t bps; /// breakpoints
} ljd_t;

/// init debugger object
#define LJD_INIT (ljd_t){ \
  .coro = LUA_REFNIL, \
  .func = LUA_REFNIL, \
  .currentbp = 0, \
  .currentline = -1, \
  .currentsrc = NULL, \
  .skipline = -1, \
  .skiplevel = -1, \
  .continuing = false, \
  .locals = LOCALS_INIT, \
  .bps = BPS_INIT, \
}



/// get call stack level
static int getlevel(lua_State *L)
{
  lua_Debug ar;
  int level = 0;
  while (lua_getstack(L, level, &ar))
    ++level;
  return level;
}

/// coroutine status
typedef enum {
  CORO_DEAD = 0,
  CORO_NORMAL = 1,
  CORO_SUSPENDED = 2,
} coro_status_t;

/// get coroutine status
static coro_status_t coro_status(lua_State *L)
{
  int status = lua_status(L);
  if (status == LUA_YIELD) {
    return CORO_SUSPENDED;
  } else if (status == LUA_OK) {
    lua_Debug ar;
    if (lua_getstack(L, 0, &ar) > 0) {
      return CORO_NORMAL;
    } else if (lua_gettop(L) == 0) {
      return CORO_DEAD;
    } else {
      return CORO_SUSPENDED;
    }
  } else {
    return CORO_DEAD;
  }
}

/// can resume from coroutine
static bool canresume(lua_State *L)
{
  return coro_status(L) != CORO_DEAD;
}


static void hook(lua_State *L, lua_Debug *ar)
{
  if (ar->event != LUA_HOOKLINE)
    return;

  const int top = lua_gettop(L);

  lua_getfield(L, LUA_REGISTRYINDEX, LJD_RUNNING);
  ljd_t *ljd = luaL_checkudata(L, -1, LJD_DEBUGGER);

  // luajit ignores thread in sethook, we have to check this outselves.
  // I think this might be actually useless though, since we're resetting
  // the hook function immediately after it yields.
  // TODO: handle coroutines, somehow?
  lua_rawgeti(L, LUA_REGISTRYINDEX, ljd->coro);
  if (lua_tothread(L, -1) != L) {
    lua_settop(L, top);
    return;
  }

  // we have pointer to ljd, it's referenced in registry, so it should be
  // fine to clean up the stack here.
  lua_settop(L, top);

  // save current line
  ljd->currentline = ar->currentline;

  // handle breakpoints only when continuing (this is wrong)
  if (ljd->continuing) { // TODO: move to a separate hook function
    if (ljd->bps.len == 0 || !CAN_YIELD(L))
      return;
    bool gotinfo = false;
    for (size_t i = 0; i < ljd->bps.len; ++i) {
      bp_t *bp = &ljd->bps.data[i];
      // check line first, since it's cheap
      if (ar->currentline < 1 || ar->currentline != bp->line)
        return;

      // get source info if necessary, once
      if (!gotinfo) {
        if (!lua_getinfo(L, "S", ar))
          return;
        gotinfo = true;
      }

      // skip if we don't know the file
      if (ar->source == NULL || *(ar->source) != '@')
        continue;

      // compare source files after the '@'
      if (strcmp(bp->file, ar->source + 1) == 0) {
        if (ar->currentline != ljd->skipline) {
          ljd->skipline = ar->currentline;
          ljd->currentbp = bp->id;
          size_t len = strlen(ar->source);
          char *source = malloc(len + 1);
          if (source != NULL) {
            assert(ljd->currentsrc == NULL);
            memcpy(source, ar->source, len + 1);
            ljd->currentsrc = source;
          }

          locals_clear(&ljd->locals);
          for (int i = 1;; ++i) {
            const char *name = lua_getlocal(L, ar, i);
            if (name == NULL)
              break;
            locals_push(&ljd->locals, name);
            lua_pop(L, 1);
          }
          lua_yield(L, 0);
          return;
        } else {
          ljd->skipline = -1;
        }
      }
    }
  }

  // skip function (next or finish)
  // TODO: change hook to LUA_HOOKRET
  if (ljd->skiplevel != -1 && getlevel(L) > ljd->skiplevel)
    return;

  if (ar->currentline != ljd->skipline) {
    // hook is called before the line is executed,
    // so to not fall into a loop, save the line
    // number and skip it next time
    // TODO: save source, because it could be a different file
    ljd->skipline = ar->currentline;
    ljd->currentbp = 0;
    if (lua_getinfo(L, "S", ar) && ar->source != NULL) {
      // TODO: ignore if doesn't start with '@'?
      size_t len = strlen(ar->source);
      char *source = malloc(len + 1);
      if (source != NULL) {
        assert(ljd->currentsrc == NULL);
        memcpy(source, ar->source, len + 1);
        ljd->currentsrc = source;
      }
    }

    if (CAN_YIELD(L)) {
      locals_clear(&ljd->locals);
      for (int i = 1;; ++i) {
        const char *name = lua_getlocal(L, ar, i);
        if (name == NULL)
          break;
        locals_push(&ljd->locals, name);
        lua_pop(L, 1);
      }
      lua_yield(L, 0);
      return;
    }
  } else {
    ljd->skipline = -1;
  }
}


static inline bool action_enter(lua_State *L, ljd_t **ljd, lua_State **coro)
{
  *ljd = luaL_checkudata(L, 1, LJD_DEBUGGER);
  lua_rawgeti(L, LUA_REGISTRYINDEX, (*ljd)->coro);
  *coro = lua_tothread(L, 2);

  if (L == *coro) {
    luaL_error(L, "cannot resume main thread");
    return false;
  }

  if (!canresume(*coro)) {
    luaL_error(L, "cannot resume dead coroutine");
    return false;
  }

  // reset state
  (*ljd)->currentbp = 0;
  if ((*ljd)->currentsrc != NULL) {
    free((*ljd)->currentsrc);
    (*ljd)->currentsrc = NULL;
  }

  (*ljd)->skiplevel = -1;
  (*ljd)->continuing = false;

  // save current debugger
  lua_pushvalue(L, 1);
  lua_setfield(L, LUA_REGISTRYINDEX, LJD_RUNNING);
  return true;
}

static inline int action_return(lua_State *L, ljd_t *ljd, lua_State *coro, int status)
{
  ljd->skiplevel = -1;
  ljd->continuing = false;

  // unset current debugger
  lua_pushnil(L);
  lua_setfield(L, LUA_REGISTRYINDEX, LJD_RUNNING);

  if (status == LUA_OK) { // coroutine returned
    lua_pushboolean(L, 1);
    return 1;
  } else if (status == LUA_YIELD) {
    lua_pushnumber(L, ljd->currentbp);
    if (ljd->currentline < 0)
      return 1;
    lua_pushnumber(L, ljd->currentline);
    if (ljd->currentsrc == NULL)
      return 2;
    lua_pushstring(L, ljd->currentsrc);
    return 3;
  } else if (status == LUA_ERRRUN) {
    lua_pushboolean(L, 0);
    lua_xmove(coro, L, 1); // move error message
    return 2;
  }

  // TODO: handle other statuses
  return luaL_error(L, "unknown resume status");
}

/// ljd.step
static int ljd_step(lua_State *L)
{
  ljd_t *ljd;
  lua_State *coro;
  if (!action_enter(L, &ljd, &coro))
    return 0;

  lua_sethook(coro, hook, LUA_MASKLINE, 0);
  int status = lua_resume(coro, 0);
  lua_sethook(coro, NULL, 0, 0); // TODO: restore previous hook

  return action_return(L, ljd, coro, status);
}

/// ljd.next
static int ljd_next(lua_State *L)
{
  ljd_t *ljd;
  lua_State *coro;
  if (!action_enter(L, &ljd, &coro))
    return 0;

  ljd->skiplevel = getlevel(coro);

  lua_sethook(coro, hook, LUA_MASKLINE, 0);
  int status = lua_resume(coro, 0);
  lua_sethook(coro, NULL, 0, 0); // TODO: restore previous hook

  return action_return(L, ljd, coro, status);
}

/// ljd.finish
static int ljd_finish(lua_State *L)
{
  ljd_t *ljd;
  lua_State *coro;
  if (!action_enter(L, &ljd, &coro))
    return 0;

  ljd->skiplevel = getlevel(coro) - 1;
  if (ljd->skiplevel < 0)
    ljd->skiplevel = 0;

  lua_sethook(coro, hook, LUA_MASKLINE, 0);
  int status = lua_resume(coro, 0);
  lua_sethook(coro, NULL, 0, 0); // TODO: restore previous hook

  return action_return(L, ljd, coro, status);
}

/// ljd.continue
static int ljd_continue(lua_State *L)
{
  ljd_t *ljd;
  lua_State *coro;
  if (!action_enter(L, &ljd, &coro))
    return 0;

  ljd->continuing = true;

  int status;
  if (ljd->bps.len > 0) { // set hook if there are any breakpoints set
    lua_sethook(coro, hook, LUA_MASKLINE, 0);
    status = lua_resume(coro, 0);
    lua_sethook(coro, NULL, 0, 0); // TODO: restore previous hook
  } else {
    status = lua_resume(coro, 0);
  }

  return action_return(L, ljd, coro, status);
}



static void hook_one(lua_State *L, lua_Debug *ar)
{
  (void)ar;
  lua_sethook(L, NULL, 0, 0); // TODO: restore previous hook
  if (CAN_YIELD(L))
    lua_yield(L, 0);
}

/// ljd.breakpoint: create a new breakpoint on the current line
static int ljd_bp(lua_State *L)
{
  lua_settop(L, 0);

  lua_getfield(L, LUA_REGISTRYINDEX, LJD_RUNNING);
  ljd_t *data = lua_touserdata(L, 1);
  if (data == NULL || !lua_getmetatable(L, 1))
    return 0;
  luaL_getmetatable(L, LJD_DEBUGGER);
  if (!lua_rawequal(L, 1, 2))
    return 0;
  lua_pop(L, 2);

  lua_Debug ar;
  if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "lS", &ar)) {
    data->currentline = ar.currentline;
    size_t len = strlen(ar.source);
    char *source = malloc(len + 1);
    if (source != NULL) {
      assert(data->currentsrc == NULL);
      memcpy(source, ar.source, len + 1);
      data->currentsrc = source;
    }
  }

  // if we yield from here, the info is gone, can't access
  // locals etc, so wait one instruction and yield from hook
  lua_sethook(L, hook_one, LUA_MASKCOUNT, 1);
  return 0;
}

/// ljd.breakpoint: create a new breakpoint
static int ljd_bp_add(lua_State *L)
{
  ljd_t *ljd = luaL_checkudata(L, 1, LJD_DEBUGGER);
  const char *file = luaL_checkstring(L, 2);
  int line = luaL_checkint(L, 3);

  const char *err;
  uint32_t id = bps_add(&ljd->bps, file, line, &err);
  if (id == 0)
    return luaL_error(L, err);

  lua_pushinteger(L, id);
  return 1;
}



/// ljd.new: create a new debugger
static int ljd_new(lua_State *L)
{
  luaL_checktype(L, 1, LUA_TFUNCTION);

  lua_State *coro = lua_newthread(L);
  lua_pushvalue(L, 1);
  lua_xmove(L, coro, 1); // copy function to the new thread

  ljd_t *data = lua_newuserdata(L, sizeof(ljd_t));
  *data = LJD_INIT;
  lua_pushvalue(L, 1); // save function ref
  data->func = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pushvalue(L, -2); // save thread ref
  data->coro = luaL_ref(L, LUA_REGISTRYINDEX);

  luaL_getmetatable(L, LJD_DEBUGGER);
  lua_setmetatable(L, -2);
  return 1;
}

/// __gc
static int ljd_gc(lua_State *L)
{
  ljd_t *ljd = luaL_checkudata(L, 1, LJD_DEBUGGER);

  luaL_unref(L, LUA_REGISTRYINDEX, ljd->func);
  ljd->func = LUA_REFNIL;
  luaL_unref(L, LUA_REGISTRYINDEX, ljd->coro);
  ljd->coro = LUA_REFNIL;

  free(ljd->currentsrc);
  ljd->currentsrc = NULL;

  locals_free(&ljd->locals);
  bps_free(&ljd->bps);

  return 0;
}

/// ljd:locals()
///
/// names of locals have to be accessed through this
/// because they are not refreshed to the outside,
/// either because of the hook or yielding from it
static int ljd_locals(lua_State *L)
{
  ljd_t *ljd = luaL_checkudata(L, 1, LJD_DEBUGGER);
  int level = luaL_checkinteger(L, 2);
  if (level < 0)
    return luaL_error(L, "invalid level");

  if (level == 0) {
    lua_newtable(L);
    if (ljd->locals.data == NULL)
      return 1;

    for (size_t i = 1, pos = 0; pos < ljd->locals.len; ++i) {
      const size_t len = strlen(ljd->locals.data + pos);
      lua_pushlstring(L, ljd->locals.data + pos, len);
      lua_rawseti(L, -2, i);
      pos += len + 1;
    }
  } else {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ljd->coro);
    lua_State *coro = lua_tothread(L, -1);

    lua_Debug ar;
    if (!lua_getstack(coro, level, &ar)) {
      lua_pushnil(L);
      return 1;
    }

    lua_newtable(L);
    for (int i = 1;; ++i) {
      const char *name = lua_getlocal(coro, &ar, i);
      if (name == NULL)
        break;
      lua_pushstring(L, name);
      lua_rawseti(L, -2, i);
      lua_pop(coro, 1);
    }
  }
  return 1;
}

/// __index
static int ljd_index(lua_State *L)
{
  ljd_t *ljd = luaL_checkudata(L, 1, LJD_DEBUGGER);
  const char *index = luaL_checkstring(L, 2);
  lua_settop(L, 2);

  if (strcmp(index, "coro") == 0) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ljd->coro);
  } else if (strcmp(index, "func") == 0) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ljd->func);
  } else if (strcmp(index, "status") == 0) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ljd->coro);
    lua_State *coro = lua_tothread(L, -1);
    if (L == coro) {
      lua_pushliteral(L, "running");
    } else {
      switch (coro_status(coro)) {
      case CORO_DEAD:
        lua_pushliteral(L, "dead");
        break;
      case CORO_NORMAL:
        lua_pushliteral(L, "normal");
        break;
      case CORO_SUSPENDED:
        lua_pushliteral(L, "suspended");
        break;
      }
    }
  } else if (strcmp(index, "next") == 0) {
    lua_pushcfunction(L, ljd_next);
  } else if (strcmp(index, "step") == 0) {
    lua_pushcfunction(L, ljd_step);
  } else if (strcmp(index, "finish") == 0) {
    lua_pushcfunction(L, ljd_finish);
  } else if (strcmp(index, "continue") == 0) {
    lua_pushcfunction(L, ljd_continue);
  } else if (strcmp(index, "currentline") == 0) {
    lua_pushinteger(L, ljd->currentline);
  } else if (strcmp(index, "locals") == 0) {
    lua_pushcfunction(L, ljd_locals);
  } else if (strcmp(index, "breakpoint") == 0) {
    lua_pushcfunction(L, ljd_bp_add);
  } else {
    return 0;
  }
  return 1;
}

/// __tostring
static int ljd_tostring(lua_State *L)
{
  lua_pushfstring(L, "ljd: %p", luaL_checkudata(L, 1, LJD_DEBUGGER));
  return 1;
}



int CONCAT(luaopen_, MODNAME)(lua_State *L)
{
  luaL_newmetatable(L, LJD_DEBUGGER);
  lua_pushcfunction(L, ljd_gc);
  lua_setfield(L, -2, "__gc");
  lua_pushcfunction(L, ljd_index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, ljd_tostring);
  lua_setfield(L, -2, "__tostring");
  lua_pop(L, 1);

  lua_createtable(L, 0, 1);
  lua_pushcfunction(L, ljd_new);
  lua_setfield(L, -2, "new");
  lua_pushcfunction(L, ljd_bp);
  lua_setfield(L, -2, "breakpoint");
  return 1;
}

// vim: sw=2 sts=2 et
