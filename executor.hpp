#pragma once

#include <mach-o/dyld.h>
#include <string>
#include <cstdint>
#include <cstdio>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// executor.hpp  ·  Antigravity Script Executor
//
// Treats the Roblox Lua VM like an API:
//   init()     — wire up all function pointers from the live binary
//   execute()  — compile + run a Lua script in a sandboxed thread
// ─────────────────────────────────────────────────────────────────────────────

namespace executor {

// ── Helpers ──────────────────────────────────────────────────────────────────

// Rebase a Mach-O compile-time address to where Roblox is actually loaded.
static inline uintptr_t rebase(uintptr_t addr) {
    return (uintptr_t)_dyld_get_image_header(0) + (addr - 0x100000000ULL);
}

// ── Raw addresses (from your dump) ───────────────────────────────────────────
// Format: Mach-O absolute address as seen in IDA/Binary Ninja.
// rebase() converts them to runtime pointers at init time.

// State / Error
static constexpr uintptr_t ADDR_GetGlobalState    = 0x1017C1857; // ScriptContext singleton accessor
static constexpr uintptr_t ADDR_ThrowError         = 0x1044C2D78; // Fatal logger / C++ exception trigger

// Stack management (lapi.cpp)
static constexpr uintptr_t ADDR_Lua_GetTop         = 0x10407E8A3; // returns # items on stack
static constexpr uintptr_t ADDR_Lua_CheckStack      = 0x10407E613; // grow stack if needed
static constexpr uintptr_t ADDR_Lua_PushValue       = 0x10407EAF1; // copy item[idx] → top
static constexpr uintptr_t ADDR_Lua_Remove          = 0x10407E8B5; // remove item in middle
static constexpr uintptr_t ADDR_Lua_SetTop          = 0x10407E8A3; // set stack top / pop N items

// Execution
static constexpr uintptr_t ADDR_Lua_GetGlobal       = 0x10407FBDD; // push globals["name"] → top
static constexpr uintptr_t ADDR_Lua_PCall           = 0x10408074B; // protected call (safety net)
static constexpr uintptr_t ADDR_LuaToLString        = 0x10407F14E; // read string from stack

// Thread
static constexpr uintptr_t ADDR_Lua_NewThread       = 0x10405333D; // spawn sandboxed coroutine

// Bytecode loader — "RSB1" string → function called twice below it
static constexpr uintptr_t ADDR_Luau_Load           = 0x104077DBB; // deserialize bytecode → closure

// Logging
static constexpr uintptr_t ADDR_Print               = 0x1001DD7B4; // variadic printf to console

// Identity — thread security level (from your source: lua_State + 0x78 → *ptr + 0x30)
// We use ScriptContext::SetThreadIdentity to elevate to level 8 (LocalUser/Plugin)
static constexpr uintptr_t ADDR_SetThreadIdentity   = 0x1017C6784; // getglobalstate / identity setter

// ── Function pointer typedefs ─────────────────────────────────────────────────

using fn_GetGlobalState_t    = uintptr_t  (*)();
using fn_ThrowError_t        = void       (*)(const char* msg);
using fn_lua_gettop_t        = int        (*)(uintptr_t L);
using fn_lua_checkstack_t    = int        (*)(uintptr_t L, int sz);
using fn_lua_pushvalue_t     = void       (*)(uintptr_t L, int idx);
using fn_lua_remove_t        = void       (*)(uintptr_t L, int idx);
using fn_lua_settop_t        = void       (*)(uintptr_t L, int idx);
using fn_lua_getglobal_t     = void       (*)(uintptr_t L, const char* name);
using fn_lua_pcall_t         = int        (*)(uintptr_t L, int nargs, int nresults, int msgh);
using fn_lua_tolstring_t     = const char*(*)(uintptr_t L, int idx, size_t* len);
using fn_lua_newthread_t     = uintptr_t  (*)(uintptr_t L);
using fn_luau_load_t         = int        (*)(uintptr_t L, const char* chunkname,
                                               const char* data, size_t size, int env);
using fn_print_t             = void       (*)(int level, const char* fmt, ...);

// ── Live function pointers (set by init()) ────────────────────────────────────

static fn_GetGlobalState_t   pfn_GetGlobalState   = nullptr;
static fn_ThrowError_t       pfn_ThrowError        = nullptr;
static fn_lua_gettop_t       pfn_lua_gettop        = nullptr;
static fn_lua_checkstack_t   pfn_lua_checkstack    = nullptr;
static fn_lua_pushvalue_t    pfn_lua_pushvalue      = nullptr;
static fn_lua_remove_t       pfn_lua_remove        = nullptr;
static fn_lua_settop_t       pfn_lua_settop        = nullptr;
static fn_lua_getglobal_t    pfn_lua_getglobal     = nullptr;
static fn_lua_pcall_t        pfn_lua_pcall         = nullptr;
static fn_lua_tolstring_t    pfn_lua_tolstring     = nullptr;
static fn_lua_newthread_t    pfn_lua_newthread     = nullptr;
static fn_luau_load_t        pfn_luau_load         = nullptr;
static fn_print_t            pfn_print             = nullptr;

// ── Shared state ─────────────────────────────────────────────────────────────

static uintptr_t g_lua_state  = 0;   // main lua_State — must be set externally
static bool      g_ready      = false;
static char      g_status[512] = "Idle. Waiting for game...";
static bool      g_last_ok    = false;

// ── Identity helper ───────────────────────────────────────────────────────────
// Thread identity is at: *(int128*)( *((uintptr_t*)(state + 0x78)) + 0x30 )
// Level 8 = full "LocalUser / Plugin" permissions.
static void elevate_identity(uintptr_t thread) {
    uintptr_t extra_space_ptr = *(uintptr_t*)(thread + 0x78);
    if (!extra_space_ptr) return;
    __int128* identity = (__int128*)(extra_space_ptr + 0x30);
    *identity = 8;
}

// ── init() — wire up all function pointers ────────────────────────────────────
static void init() {
    if (g_ready) return;

    pfn_GetGlobalState  = (fn_GetGlobalState_t)  rebase(ADDR_GetGlobalState);
    pfn_ThrowError       = (fn_ThrowError_t)       rebase(ADDR_ThrowError);
    pfn_lua_gettop       = (fn_lua_gettop_t)       rebase(ADDR_Lua_GetTop);
    pfn_lua_checkstack   = (fn_lua_checkstack_t)   rebase(ADDR_Lua_CheckStack);
    pfn_lua_pushvalue    = (fn_lua_pushvalue_t)    rebase(ADDR_Lua_PushValue);
    pfn_lua_remove       = (fn_lua_remove_t)       rebase(ADDR_Lua_Remove);
    pfn_lua_settop       = (fn_lua_settop_t)       rebase(ADDR_Lua_SetTop);
    pfn_lua_getglobal    = (fn_lua_getglobal_t)    rebase(ADDR_Lua_GetGlobal);
    pfn_lua_pcall        = (fn_lua_pcall_t)        rebase(ADDR_Lua_PCall);
    pfn_lua_tolstring    = (fn_lua_tolstring_t)    rebase(ADDR_LuaToLString);
    pfn_lua_newthread    = (fn_lua_newthread_t)    rebase(ADDR_Lua_NewThread);
    pfn_luau_load        = (fn_luau_load_t)        rebase(ADDR_Luau_Load);
    pfn_print            = (fn_print_t)            rebase(ADDR_Print);

    g_ready = true;
}

// ── execute() — the full "stack dance" ───────────────────────────────────────
//
// Flow (mirrors the conversation in your docs):
//   1. CheckStack   — make sure we won't overflow
//   2. NewThread    — isolated coroutine (crash here = game survives)
//   3. elevate      — set identity = 8 so scripts can call Roblox APIs
//   4. luau_load    — deserialize Lua source into a closure on the thread stack
//   5. lua_pcall    — execute (0 args, 0 results, no error handler needed)
//   6. tolstring    — read error if call failed
//   7. settop       — clean up the thread stack
//
static bool execute(const std::string& source) {
    init();

    if (!g_lua_state) {
        snprintf(g_status, sizeof(g_status),
                 "ERR: No Lua state captured. Join a game first.");
        g_last_ok = false;
        return false;
    }

    uintptr_t L = g_lua_state;

    // ── Step 1: Guarantee stack headroom ─────────────────────────────────────
    pfn_lua_checkstack(L, 32);
    int saved_top = pfn_lua_gettop(L);

    // ── Step 2: Spawn sandboxed coroutine ────────────────────────────────────
    // NewThread pushes the thread onto L's stack AND returns it.
    // We execute on the new thread so a runtime error won't nuke the main state.
    uintptr_t thread = pfn_lua_newthread(L);
    if (!thread) {
        snprintf(g_status, sizeof(g_status), "ERR: lua_newthread returned null.");
        g_last_ok = false;
        pfn_lua_settop(L, saved_top);
        return false;
    }

    // ── Step 3: Elevate identity so Roblox APIs are accessible ───────────────
    elevate_identity(thread);

    // ── Step 4: Load (compile) the source into a closure ─────────────────────
    // luau_load expects Luau bytecode. Roblox's internal loader will also accept
    // raw Lua source and call the internal compiler. Chunk name "=Antigravity"
    // is cosmetic (shows up in error messages / stack traces).
    int load_err = pfn_luau_load(thread,
                                  "=Antigravity",
                                  source.c_str(),
                                  source.size(),
                                  0);  // env=0 → use thread's globals

    if (load_err != 0) {
        size_t len = 0;
        const char* msg = pfn_lua_tolstring(thread, -1, &len);
        snprintf(g_status, sizeof(g_status), "COMPILE ERR: %s",
                 (msg && len) ? msg : "(no message)");
        g_last_ok = false;
        pfn_lua_settop(L, saved_top);
        return false;
    }

    // At this point the closure is sitting at the top of 'thread's stack.

    // ── Step 5: Protected call — the safety net ───────────────────────────────
    // Lua_PCall(L, nargs=0, nresults=0, msgh=0)
    //   nargs=0   → the function takes no arguments
    //   nresults=0 → we don't care about return values
    //   msgh=0    → no custom error handler (simple error string is enough)
    int call_err = pfn_lua_pcall(thread, 0, 0, 0);

    if (call_err != 0) {
        // ── Step 6: Read the error from the stack ────────────────────────────
        size_t len = 0;
        const char* msg = pfn_lua_tolstring(thread, -1, &len);
        snprintf(g_status, sizeof(g_status), "RUNTIME ERR: %s",
                 (msg && len) ? msg : "(no message)");
        g_last_ok = false;
        // Log to Roblox console as well
        if (pfn_print) pfn_print(3, "[Antigravity] %s", g_status);
        pfn_lua_settop(L, saved_top);
        return false;
    }

    // ── Step 7: Clean up ─────────────────────────────────────────────────────
    pfn_lua_settop(L, saved_top);

    snprintf(g_status, sizeof(g_status), "OK  Script executed successfully.");
    g_last_ok = true;
    if (pfn_print) pfn_print(0, "[Antigravity] Script executed.");
    return true;
}

// ── Quick-test helper: run print("Hello from Antigravity!") ─────────────────
static bool self_test() {
    return execute(R"(print("Hello from Antigravity! Identity: " .. tostring(identityOf(coroutine.running()))))");
}

} // namespace executor
