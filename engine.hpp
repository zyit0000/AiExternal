#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// engine.hpp  ·  Antigravity Engine Map
//
// Complete namespace of every known offset and function, plus the logic to
// resolve DataModel and LocalPlayer via the TaskScheduler job chain.
//
// Resolution chain:
//   raw_taskscheduler  →  iterate job list  →  WaitingHybridScriptsJob
//     → Get_DataModel_Internal()  →  DataModel
//       → Players service  →  LocalPlayer
// ─────────────────────────────────────────────────────────────────────────────

#include <mach-o/dyld.h>
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace engine {

// ── Rebase helper ─────────────────────────────────────────────────────────────
static inline uintptr_t rebase(uintptr_t addr) {
    return (uintptr_t)_dyld_get_image_header(0) + (addr - 0x100000000ULL);
}

// ─────────────────────────────────────────────────────────────────────────────
// NAMESPACE: EngineOffsets
// Raw Mach-O addresses from IDA dump.
// ─────────────────────────────────────────────────────────────────────────────
namespace EngineOffsets {
    // ── Functions ─────────────────────────────────────────────────────────────
    // DataModel accessor. Signature: uintptr_t(uintptr_t JobContext)
    // Checks DMLock before returning. Context → Scheduler → DataModel.
    static constexpr uintptr_t Get_DataModel_Internal        = 0x10401F968;

    // Climbs Job hierarchy (+0x70) and subtracts 440 (0x1B8) to find the
    // TaskScheduler base from any child job. Signature: uintptr_t(uintptr_t job)
    static constexpr uintptr_t Resolve_Scheduler_From_Job    = 0x10401FA76;

    // Constructor / initialiser for the WaitingHybridScriptsJob — the job
    // that owns the DataModel and drives all Lua script execution.
    static constexpr uintptr_t Init_WaitingHybridScriptsJob  = 0x10180ADCE;

    // Diagnostic: iterates all current jobs in the TaskScheduler for crash reports.
    static constexpr uintptr_t Get_All_Scheduler_Jobs        = 0x104460A0E;

    // Wrapper that retrieves a specific pointer from a Job context.
    static constexpr uintptr_t Static_DataModel_Getter_Proxy = 0x1000FD3F8;

    // Get LocalPlayer directly from context/job
    static constexpr uintptr_t Get_LocalPlayer_From_Context  = 0x1026D445E;

    // ── Lua VM ───────────────────────────────────────────────────────────────
    static constexpr uintptr_t GetGlobalState       = 0x1017C1857; // ScriptContext lua_State accessor
    static constexpr uintptr_t Lua_GetTop           = 0x10407E8A3;
    static constexpr uintptr_t Lua_CheckStack       = 0x10407E613;
    static constexpr uintptr_t Lua_PushValue        = 0x10407EAF1;
    static constexpr uintptr_t Lua_Remove           = 0x10407E8B5;
    static constexpr uintptr_t Lua_GetGlobal        = 0x10407FBDD;
    static constexpr uintptr_t Lua_PCall            = 0x10408074B;
    static constexpr uintptr_t LuaToLString         = 0x10407F14E;
    static constexpr uintptr_t LuaL_newstate        = 0x1040912A0; // Allocates fresh lua_State sandbox
    static constexpr uintptr_t ThrowError           = 0x1044C2D78; // Fatal logger → C++ exception
    static constexpr uintptr_t Print                = 0x1001DD7B4; // Variadic printf to console
    static constexpr uintptr_t StringInit           = 0x10177D0B8; // std::string constructor (ctor)
    static constexpr uintptr_t StdString_Deallocate = 0x1044BCD08; // std::string destructor (dtor)

    // ── Global variables / singletons ────────────────────────────────────────
    // The raw TaskScheduler singleton pointer (read this address to get TS).
    static constexpr uintptr_t raw_taskscheduler    = 0x106cce5c8;

    // Boolean flag: 1 if DataModel is locked/initialised, 0 otherwise.
    // Must be 1 before calling Get_DataModel_Internal.
    static constexpr uintptr_t DMLock_State         = 0x106158DD8;

    // Logging level for crash/diagnostic output.
    static constexpr uintptr_t FLog_CrashReport     = 0x105EF1880;
}

// ─────────────────────────────────────────────────────────────────────────────
// NAMESPACE: Offsets — struct field offsets, mirroring your complete dump
// ─────────────────────────────────────────────────────────────────────────────
namespace Offsets {
    // TaskScheduler job list (std::vector)
    static constexpr int TS_JobStart        = 0xD0;   // uintptr_t* begin
    static constexpr int TS_JobEnd          = 0xD8;   // uintptr_t* end
    static constexpr int TS_MaxFps          = 0xB8;

    // Job struct
    static constexpr int Job_Name           = 0x18;   // const char* / std::string
    static constexpr int Job_Name_external  = 0x28;
    static constexpr int Job_Parent         = 0x70;   // (+112) pointer to parent / owner
    static constexpr int Job_ScriptContext  = 0x1A0;  // WaitingHybridScriptsJob→ScriptContext

    // Scheduler→job distance
    static constexpr int Scheduler_To_RootJob = 440; // (0x1B8)

    // DataModel
    static constexpr int DMLock_Check       = 0x108;  // per-instance DM lock flag
    static constexpr int DM_Workspace       = 0x168;
    static constexpr int DM_PlaceId         = 0x6D0;
    static constexpr int DM_GameId          = 0x6C8;
    static constexpr int DM_JobId           = 0x138;

    // Instance (common to all RBX objects)
    static constexpr int Inst_ChildStart    = 0x78;
    static constexpr int Inst_ChildEnd      = 0x80;
    static constexpr int Inst_Parent        = 0x70;
    static constexpr int Inst_Name          = 0xB0;
    static constexpr int Inst_ClassDesc     = 0x18;
    static constexpr int Inst_ClassName     = 0x08;

    // Players service
    static constexpr int Players_LocalPlayer= 0x120;

    // LocalPlayer / Player
    static constexpr int LP_Character       = 0x350;
    static constexpr int LP_DisplayName     = 0xB0;
    static constexpr int LP_Team            = 0x258;

    // Humanoid
    static constexpr int Hum_WalkSpeed      = 0x1C8; // float
    static constexpr int Hum_Health         = 0x178; // float
    static constexpr int Hum_JumpPower      = 0x1A0; // float

    // Proto (Luau bytecode structure)
    static constexpr int Proto_code         = 0x18;
    static constexpr int Proto_sizecode     = 0x98;
    static constexpr int Proto_constants    = 0x08;
    static constexpr int Proto_sizek        = 0x94;
    static constexpr int Proto_protos       = 0x10;
    static constexpr int Proto_sizep        = 0xA0;
    static constexpr int Proto_nupvalues    = 0xA8;

    // lua_State
    static constexpr int State_base         = 0x08;
    static constexpr int State_top          = 0x18;
    static constexpr int State_callinfo     = 0x28;
    static constexpr int State_callbase     = 0x40;
    static constexpr int State_env          = 0x00;

    // CallInfo
    static constexpr int CI_func            = 0x10;
    static constexpr int CI_base            = 0x00;
    static constexpr int CI_top             = 0x08;

    // Thread identity (ExtraSpace chain)
    static constexpr int Thread_ExtraSpace  = 0x78;  // state + 0x78 → ptr
    static constexpr int ExtraSpace_Identity= 0x30;  // *ptr + 0x30 → __int128

    // Signal
    static constexpr int Signal_next        = 0x20;
    static constexpr int Signal_state       = 0x28;
}

// ─────────────────────────────────────────────────────────────────────────────
// Runtime resolution helpers
// ─────────────────────────────────────────────────────────────────────────────

using fn_GetDataModel_t       = uintptr_t(*)(uintptr_t job_context);
using fn_GetLocalPlayer_t     = uintptr_t(*)(uintptr_t job_context);

// Cached resolved pointers
static uintptr_t g_datamodel     = 0;
static uintptr_t g_local_player  = 0;
static uintptr_t g_taskscheduler = 0;

// ── get_taskscheduler() ───────────────────────────────────────────────────────
// Reads the global singleton pointer that the engine maintains for the TS.
// raw_taskscheduler is the address OF the pointer, not the pointer itself.
static uintptr_t get_taskscheduler() {
    if (g_taskscheduler) return g_taskscheduler;
    uintptr_t ts_ptr_addr = rebase(EngineOffsets::raw_taskscheduler);
    g_taskscheduler = *(uintptr_t*)ts_ptr_addr;
    return g_taskscheduler;
}

// ── find_datamodel() ─────────────────────────────────────────────────────────
// Step-by-step:
//   1. Check DMLock — the engine asserts this is 1 before serving the DM.
//   2. Read the TaskScheduler singleton.
//   3. Walk the job list (std::vector of pointers at TS + 0xD0 .. 0xD8).
//   4. For each job, read the name at job + 0x18.
//   5. When we hit "WaitingHybridScriptsJob", call Get_DataModel_Internal(job).
static uintptr_t find_datamodel() {
    if (g_datamodel) return g_datamodel;

    // 1. DMLock check
    uintptr_t dmlock_addr = rebase(EngineOffsets::DMLock_State);
    if (!dmlock_addr || !*(uint8_t*)dmlock_addr) return 0;

    // 2. TaskScheduler
    uintptr_t ts = get_taskscheduler();
    if (!ts) return 0;

    // 3. Job vector bounds
    uintptr_t job_start = *(uintptr_t*)(ts + Offsets::TS_JobStart);
    uintptr_t job_end   = *(uintptr_t*)(ts + Offsets::TS_JobEnd);
    if (!job_start || job_start >= job_end) return 0;

    auto fn_GetDM = (fn_GetDataModel_t)rebase(EngineOffsets::Get_DataModel_Internal);

    // 4 & 5. Iterate jobs
    for (uintptr_t cur = job_start; cur < job_end; cur += sizeof(uintptr_t)) {
        uintptr_t job = *(uintptr_t*)cur;
        if (!job) continue;

        // Name is a std::string at job + 0x18. Read the pointer inside it.
        // On macOS ARM64, small std::string stores chars inline in the struct.
        // Safe: just cast to char* directly at the offset.
        const char* name = reinterpret_cast<const char*>(job + Offsets::Job_Name);
        if (!name) continue;

        if (strncmp(name, "WaitingHybridScriptsJob", 23) == 0) {
            // Call the engine's own accessor — it handles DMLock internally.
            uintptr_t dm = fn_GetDM(job);
            if (dm) {
                g_datamodel = dm;
                printf("[engine] DataModel: 0x%llX\n", (unsigned long long)dm);
                return dm;
            }
        }
    }
    return 0;
}

// ── find_local_player() ───────────────────────────────────────────────────────
// Two approaches tried in order:
//   A. Call Get_LocalPlayer_From_Context directly if we have a job pointer.
//   B. Walk DataModel → children → find "Players" service → read LocalPlayer offset.
static uintptr_t find_local_player() {
    if (g_local_player) return g_local_player;

    uintptr_t ts = get_taskscheduler();
    if (!ts) return 0;

    uintptr_t job_start = *(uintptr_t*)(ts + Offsets::TS_JobStart);
    uintptr_t job_end   = *(uintptr_t*)(ts + Offsets::TS_JobEnd);
    if (!job_start || job_start >= job_end) return 0;

    auto fn_GetLP = (fn_GetLocalPlayer_t)rebase(EngineOffsets::Get_LocalPlayer_From_Context);

    for (uintptr_t cur = job_start; cur < job_end; cur += sizeof(uintptr_t)) {
        uintptr_t job = *(uintptr_t*)cur;
        if (!job) continue;
        const char* name = reinterpret_cast<const char*>(job + Offsets::Job_Name);
        if (!name) continue;
        if (strncmp(name, "WaitingHybridScriptsJob", 23) == 0) {
            uintptr_t lp = fn_GetLP(job);
            if (lp) {
                g_local_player = lp;
                printf("[engine] LocalPlayer: 0x%llX\n", (unsigned long long)lp);
                return lp;
            }
            break;
        }
    }
    return 0;
}

// ── get_lua_state_from_scheduler() ───────────────────────────────────────────
// Alternative to GetGlobalState: read ScriptContext ptr out of
// WaitingHybridScriptsJob + 0x1A0, then call GetGlobalState on it.
using fn_GetGlobalState_t = uintptr_t(*)(uintptr_t script_context);

static uintptr_t get_lua_state_from_scheduler() {
    uintptr_t ts = get_taskscheduler();
    if (!ts) return 0;

    uintptr_t job_start = *(uintptr_t*)(ts + Offsets::TS_JobStart);
    uintptr_t job_end   = *(uintptr_t*)(ts + Offsets::TS_JobEnd);

    auto fn_GGS = (fn_GetGlobalState_t)rebase(EngineOffsets::GetGlobalState);

    for (uintptr_t cur = job_start; cur < job_end; cur += sizeof(uintptr_t)) {
        uintptr_t job = *(uintptr_t*)cur;
        if (!job) continue;
        const char* name = reinterpret_cast<const char*>(job + Offsets::Job_Name);
        if (!name) continue;
        if (strncmp(name, "WaitingHybridScriptsJob", 23) == 0) {
            uintptr_t sc = *(uintptr_t*)(job + Offsets::Job_ScriptContext);
            if (!sc) return 0;
            return fn_GGS(sc);
        }
    }
    return 0;
}

// ── poll() — call this from your background thread ───────────────────────────
// Returns true once all three pointers are resolved.
static bool poll() {
    if (!g_taskscheduler) get_taskscheduler();
    if (!g_datamodel)     find_datamodel();
    if (!g_local_player)  find_local_player();
    return g_taskscheduler && g_datamodel;
}

// ─────────────────────────────────────────────────────────────────────────────
// Workspace dumper
// ─────────────────────────────────────────────────────────────────────────────

// Safe pointer read — returns 0 on any invalid address to prevent crashes.
static inline uintptr_t safe_read_ptr(uintptr_t addr) {
    if (!addr || addr < 0x1000) return 0;
    // On macOS we can't easily VirtualQuery; assume anything above kernel is bad.
    if (addr > 0x7FFFFFFFFFFFULL) return 0;
    return *(uintptr_t*)addr;
}

// Try to read a C-string from `addr`. Returns empty on failure.
static std::string safe_read_string(uintptr_t addr, size_t max_len = 64) {
    if (!addr || addr < 0x1000 || addr > 0x7FFFFFFFFFFFULL) return "";
    char buf[65] = {};
    // Copy byte-by-byte; stop on null or invalid char.
    for (size_t i = 0; i < max_len; ++i) {
        char c = *(char*)(addr + i);
        if (c == '\0') break;
        if (c < 0x09 || (c > 0x0D && c < 0x20) || c > 0x7E) return ""; // non-printable
        buf[i] = c;
    }
    return std::string(buf);
}

// Read the object's Name.
// RBX::Name is a std::string at inst + 0xB0.
// libc++ SSO: if the top bit of byte[0x1F] of the string struct is NOT set,
// the string data is stored inline starting at inst+0xB0 (up to 22 chars).
// If it IS set, the pointer is at inst+0xB0 (first 8 bytes = char*).
static std::string read_instance_name(uintptr_t inst) {
    if (!inst) return "?";
    uintptr_t name_field = inst + Offsets::Inst_Name; // 0xB0

    // libc++ SSO flag: byte at name_field + 0x1F, bit 0x80
    uint8_t sso_flag = *(uint8_t*)(name_field + 0x1F);
    if (sso_flag & 0x80) {
        // Long string: first 8 bytes are the char*
        uintptr_t str_ptr = safe_read_ptr(name_field);
        return safe_read_string(str_ptr, 64);
    } else {
        // Short string: data inline at name_field
        return safe_read_string(name_field, 22);
    }
}

// Read the object's ClassName via its ClassDescriptor.
// inst + 0x18 → ClassDescriptor ptr
// ClassDescriptor + 0x08 → const char* className
static std::string read_instance_classname(uintptr_t inst) {
    if (!inst) return "?";
    uintptr_t desc = safe_read_ptr(inst + Offsets::Inst_ClassDesc); // 0x18
    if (!desc) return "?";
    uintptr_t name_ptr = safe_read_ptr(desc + Offsets::Inst_ClassName); // 0x08
    return safe_read_string(name_ptr, 64);
}

// Recursive walk. `depth` controls indentation. `out` receives lines.
// Max depth / node limits prevent infinite loops on corrupt data.
static void walk_instance(uintptr_t inst, int depth,
                           std::vector<std::string>& out,
                           int& total, int max_total = 2000) {
    if (!inst || depth > 16 || total >= max_total) return;

    std::string cls  = read_instance_classname(inst);
    std::string name = read_instance_name(inst);

    // Build indented line
    std::string line(depth * 2, ' ');
    line += "[" + cls + "] \"" + name + "\"";
    line += "  @0x" + []( uintptr_t v ) {
        char buf[32]; snprintf(buf, sizeof(buf), "%llX", (unsigned long long)v);
        return std::string(buf);
    }(inst);
    out.push_back(std::move(line));
    ++total;

    // Iterate children (std::vector<Instance*> at 0x78..0x80)
    uintptr_t child_start = safe_read_ptr(inst + Offsets::Inst_ChildStart); // 0x78
    uintptr_t child_end   = safe_read_ptr(inst + Offsets::Inst_ChildEnd);   // 0x80
    if (!child_start || child_start >= child_end) return;

    // Sanity: vector shouldn't be gigantic
    size_t count = (child_end - child_start) / sizeof(uintptr_t);
    if (count > 4096) return;

    for (size_t i = 0; i < count && total < max_total; ++i) {
        uintptr_t child = safe_read_ptr(child_start + i * sizeof(uintptr_t));
        if (child && child != inst) // guard against self-reference
            walk_instance(child, depth + 1, out, total, max_total);
    }
}

// Public: dump the entire Workspace into a flat list of strings.
// Returns an empty vector if the DataModel isn't resolved yet.
static std::vector<std::string> dump_workspace() {
    std::vector<std::string> result;
    if (!g_datamodel) { result.push_back("DataModel not resolved yet."); return result; }

    // Workspace is at DataModel + DM_Workspace offset (0x168 in your dump).
    // That field is a direct pointer to the Workspace Instance.
    uintptr_t ws = safe_read_ptr(g_datamodel + Offsets::DM_Workspace);
    if (!ws) {
        // Fallback: iterate DataModel children and find the one named "Workspace"
        uintptr_t cs = safe_read_ptr(g_datamodel + Offsets::Inst_ChildStart);
        uintptr_t ce = safe_read_ptr(g_datamodel + Offsets::Inst_ChildEnd);
        if (cs && cs < ce) {
            size_t n = (ce - cs) / sizeof(uintptr_t);
            for (size_t i = 0; i < n && i < 64; ++i) {
                uintptr_t child = safe_read_ptr(cs + i * sizeof(uintptr_t));
                if (!child) continue;
                std::string cn = read_instance_classname(child);
                if (cn == "Workspace") { ws = child; break; }
            }
        }
    }

    if (!ws) { result.push_back("Workspace pointer is null. Are you in a game?"); return result; }

    char header[128];
    snprintf(header, sizeof(header), "Workspace @ 0x%llX", (unsigned long long)ws);
    result.push_back(header);
    result.push_back(std::string(48, '-'));

    int total = 0;
    walk_instance(ws, 0, result, total);

    char footer[64];
    snprintf(footer, sizeof(footer), "-- %d objects --", total);
    result.push_back(footer);
    return result;
}

} // namespace engine
