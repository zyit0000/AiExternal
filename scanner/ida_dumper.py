import idaapi
import idc
import idautils
import os

print("==============================================")
print("[+] Antigravity Full IDAPython Dumper - macOS")
print("==============================================")

BASE = idaapi.get_imagebase()
MACH_O_BASE = 0x100000000

def rebase(ea):
    if not ea or ea == idc.BADADDR:
        return 0
    return (ea - BASE) + MACH_O_BASE

def find_string(s):
    for item in idautils.Strings():
        if str(item) == s:
            return item.ea
    return idc.BADADDR

def xrefs_to(ea):
    return list(idautils.XrefsTo(ea, 0))

def func_containing(ea):
    f = idaapi.get_func(ea)
    return f.start_ea if f else idc.BADADDR

def find_func_by_string(s):
    """Find the function that CONTAINS a reference to string s."""
    ea = find_string(s)
    if ea == idc.BADADDR:
        return 0
    for xref in xrefs_to(ea):
        f = func_containing(xref.frm)
        if f != idc.BADADDR:
            return f
    return 0

def find_call_in_func(func_ea, n=0):
    """Find the nth CALL inside a function, return callee address."""
    count = 0
    ea = func_ea
    func = idaapi.get_func(func_ea)
    if not func:
        return 0
    end = func.end_ea
    while ea < end:
        mnem = idc.print_insn_mnem(ea)
        if mnem in ("call", "bl", "blr"):
            if count == n:
                # get operand
                target = idc.get_operand_value(ea, 0)
                return target
            count += 1
        ea = idc.next_head(ea, end)
    return 0

def find_func_via_luaL_reg(name_str):
    """
    luaL_Reg structs in __const/__data are [ptr-to-string, ptr-to-func].
    Find the string, find data xref, read the next 8 bytes as function pointer.
    """
    ea = find_string(name_str)
    if ea == idc.BADADDR:
        return 0
    for xref in xrefs_to(ea):
        seg = idc.get_segm_name(xref.frm)
        if seg in ("__const", "__data", ".const", ".data"):
            func_ptr = idc.get_qword(xref.frm + 8)
            if func_ptr and func_ptr != idc.BADADDR:
                return func_ptr
    return 0

def find_func_via_xref_caller(s, xref_index=0, call_index=0):
    """
    Find string -> get xref[xref_index] -> get the containing function ->
    then find call_index-th call inside that function (the callee).
    """
    ea = find_string(s)
    if ea == idc.BADADDR:
        return 0
    refs = xrefs_to(ea)
    if xref_index >= len(refs):
        return 0
    parent = func_containing(refs[xref_index].frm)
    if parent == idc.BADADDR:
        return 0
    return find_call_in_func(parent, call_index)

results = {}

print("[*] Dumping ScriptContext / Roblox engine functions...")
# print():  "Current identity is %d" is in printidentity. print() itself calls "tostring".
# The function that does lua_getglobal(L, "tostring") IS print/doPrint.
results["print_address"]           = find_func_by_string("tostring")
# warn():  Same as print but MESSAGE_WARNING. Look for "warning" string or near print.
results["warn_address"]            = find_func_via_luaL_reg("warn")
# tick():  Uses G3D::System::time(). No unique string - near print in ScriptContext.
results["tick_address"]            = find_func_via_luaL_reg("tick")
# time():  Uses gameTime(). Near print.
results["time_address"]            = find_func_via_luaL_reg("time")
# wait():  Calls lua_yield. Unique: look for "queueWaiter" or "wait" luaL_reg.
results["wait_address"]            = find_func_via_luaL_reg("wait")
# delay(): Like wait but timeout arg. Near wait.
results["delay_address"]           = find_func_via_luaL_reg("delay")
# spawn(): "Spawn function requires 1 argument" - actual error string in source.
results["spawn_address"]           = find_func_by_string("Spawn function requires 1 argument")
# ypcall / pcall: search luaL_Reg for "ypcall"
results["ypcall_address"]          = find_func_via_luaL_reg("ypcall")
results["pcall_address"]           = find_func_via_luaL_reg("pcall")
# printidentity(): "Current identity is %d"
results["printidentity_address"]   = find_func_by_string("Current identity is %d")
# settings(): "GlobalAdvancedSettings"
results["settings_address"]        = find_func_by_string("GlobalAdvancedSettings")
# usersettings(): "GlobalBasicSettings"
results["usersettings_address"]    = find_func_by_string("GlobalBasicSettings")
# pluginmanager(): "PluginManager" (singleton)
results["pluginmanager_address"]   = find_func_via_luaL_reg("PluginManager")
# version(): "DebugSettings::robloxVersion" or just the string "version"
results["version_address"]         = find_func_via_luaL_reg("version")
# stats(): "Stats::StatsService"
results["stats_address"]           = find_func_via_luaL_reg("Stats")
# LoadLibrary(): "Unknown library" is the error when an invalid name is passed
results["loadlibrary_address"]     = find_func_by_string("Unknown library")
# require(): module script loading
results["require_address"]         = find_func_via_luaL_reg("require")
# loadstring(): search for the string it uses
results["loadstring_address"]      = find_func_via_luaL_reg("loadstring")
# dofile(): "not implemented" error string
results["dofile_address"]          = find_func_by_string("not implemented")
# crash__(): "RBXCRASH" or the security check string
results["crash_address"]           = find_func_via_luaL_reg("crash__")
# getstate / getglobalstate: "startScript re-entrancy"
results["getstate_address"]        = find_func_by_string("startScript re-entrancy")
# newthread: "Unable to create a new thread for %s"
results["newthread_address"]       = find_func_by_string("Unable to create a new thread for %s")
# deserialize / luavm_load: "bytecode version mismatch"
results["deserialize_address"]     = find_func_by_string("bytecode version mismatch")

print("[*] Dumping Lua base library functions (lbaselib.c)...")
# assert(): "assertion failed!"
results["assert_address"]          = find_func_by_string("assertion failed!")
# tonumber(): "base out of range"
results["tonumber_address"]        = find_func_by_string("base out of range")
# getmetatable(): "__metatable"
results["getmetatable_address"]    = find_func_by_string("__metatable")
# setmetatable(): "cannot change a protected metatable"
results["setmetatable_address"]    = find_func_by_string("cannot change a protected metatable")
# getfenv(): "level must be non-negative"
results["getfenv_address"]         = find_func_by_string("level must be non-negative")
# setfenv(): "cannot change environment of given object"
results["setfenv_address"]         = find_func_by_string("cannot change environment of given object")
# collectgarbage(): "count" (Roblox only allows "count")
results["collectgarbage_address"]  = find_func_via_luaL_reg("collectgarbage")
# unpack(): "too many results to unpack"
results["unpack_address"]          = find_func_by_string("too many results to unpack")
# select(): "index out of range"
results["select_address"]          = find_func_by_string("index out of range")
# tostring(): calls __tostring metamethod
results["tostring_address"]        = find_func_via_luaL_reg("tostring")
# newproxy(): "newproxy only supports the arguments nil and true"
results["newproxy_address"]        = find_func_by_string("newproxy only supports the arguments nil and true")
# load(): "reader function must return a string"
results["load_address"]            = find_func_by_string("reader function must return a string")
# next(): table iteration - luaL_Reg lookup
results["next_address"]            = find_func_via_luaL_reg("next")
# pairs/ipairs: luaL_Reg lookup
results["pairs_address"]           = find_func_via_luaL_reg("pairs")
results["ipairs_address"]          = find_func_via_luaL_reg("ipairs")
results["xpcall_address"]          = find_func_via_luaL_reg("xpcall")

print("[*] Dumping Lua C API functions...")
# These follow the pattern from lapi.cpp (open source Luau):
# pushlstring: "string length overflow"
results["pushlstring_address"]     = find_func_by_string("string length overflow")
# pushcclosure: "C stack overflow"
results["pushcclosure_address"]    = find_func_by_string("C stack overflow")
# setmetatable via lapi: "cannot change a protected metatable"  (already have it)
# rawseti: "wrong number of arguments to 'insert'"
results["rawseti_address"]         = find_func_by_string("wrong number of arguments to 'insert'")
# newudata: "nil or boolean"
results["newudata_address"]        = find_func_by_string("nil or boolean")
# rawgeti: concat error string
results["rawgeti_address"]         = find_func_by_string("invalid value (%s) at index %d in table for 'concat'")
# tolstring: "__tostring must return a string"
results["tolstring_address"]       = find_func_by_string("'__tostring' must return a string")
# resume: "cannot resume dead coroutine"
results["resume_address"]          = find_func_by_string("cannot resume dead coroutine")
# getinfo: "no function environment for"
results["getinfo_address"]         = find_func_by_string("no function environment for")
# visitgco: "mainthread"
results["visitgco_address"]        = find_func_by_string("mainthread")
# lua_type: used in luaB_type via luaL_Reg
results["lua_type_address"]        = find_func_via_luaL_reg("type")

print("[*] Dumping coroutine library (lbaselib.c)...")
# cocreate: "Lua function expected"
results["cocreate_address"]        = find_func_by_string("Lua function expected")
# coresume: "coroutine expected"
results["coresume_address"]        = find_func_by_string("coroutine expected")
# costatus: "running" / "suspended" / "normal" / "dead" (all in same func)
results["costatus_address"]        = find_func_via_luaL_reg("status")
# wrap, yield, running via luaL_Reg
results["cowrap_address"]          = find_func_via_luaL_reg("wrap")
results["yield_address"]           = find_func_via_luaL_reg("yield")
results["corunning_address"]       = find_func_via_luaL_reg("running")

print("[*] Dumping Roblox engine specifics...")
# teleport: "The previous teleport is in proc"
results["teleport_address"]        = find_func_by_string("The previous teleport is in proc")
# private/join: "join-private-game"
results["private_address"]         = find_func_by_string("join-private-game")
# appendbool: "isTeleport"
results["appendbool_address"]      = find_func_by_string("isTeleport")
# context/RobloxEngine
results["context_address"]         = find_func_by_string("RobloxEngine")
# getidentity: near context, 4th xref, 0 args
results["getidentity_address"]     = find_func_by_string("Current identity is %d")
# fireclickdetector: "MaxActivationDistance"
results["fireclickdetector_addr"]  = find_func_by_string("MaxActivationDistance")
# appendtouch: "new overlap in different world"
results["appendtouch_addr"]        = find_func_by_string("new overlap in different world")
# taskscheduler: "SchedulerRate"
results["taskscheduler_address"]   = find_func_by_string("SchedulerRate")
# check_allowed: "HttpRequest.Url is not trusted"
results["check_allowed_address"]   = find_func_by_string("HttpRequest.Url is not trusted")
# detection1: "cacheentry::open header hash"
results["detection1_address"]      = find_func_by_string("cacheentry::open header hash")
# fmtcurl: "%s%s=%s"
results["fmtcurl_address"]         = find_func_by_string("%s%s=%s")
# luau_load / deserialize: "RSB1"
results["luau_load_address"]       = find_func_by_string("RSB1")
# newlclosure: found via Deserialize case 5 (no clean string, leave 0)
results["newlclosure_address"]     = 0

print("[*] Building output...")

# --- STATIC OFFSETS (structure offsets - don't change often) ---
STATIC_OFFSETS = """namespace offsets {
    const int jobs_start = 0x168;
    const int jobs_end = 0x170;
    const int jobs_offset = 0x10;
    int state_env = 0x0;
    int state_callinfo = 0x28; // "table overflow" -> func, func in func then first offset.
    const int state_callbase = 0x40; // "type":"thread" -> func, below in for loop.
    const int state_top = 0x18;
    const int state_base = 0x8;

    namespace callinfo {
        const int func_offset = 0x10; // "table overflow" -> func, func in func then second offset.
        const int base_offset = 0x0;  // getinfo -> above getinfo after stack checks and setlocal.
        const int top_offset = 0x8;   // "cannot resume dead coroutine" -> func(..., -1)
    }
    namespace roblox {
        const int workspace_offset = 0x300; // "NetworkStats" -> switch statement -> F8 (case 7)
        const int world_offset = 0x340;     // "NetworkStats" -> switch statement -> F8 (case 7)
        const int part_primitive = 0x148;   // appendtouch -> #1 xrefs -> second and third arguments
    }
    namespace table {
        const int isreadonly_offset = 0x7; // "attempt to modify a readonly table"
    }
    namespace hybridscriptsjob {
        const int script_context = 0x3a;
    }
    namespace global {
        const int global_offset = 0x30; // ipairs -> pushcclosure -> luaC_init whitebits
        const int white_offset = 0x28;  // ipairs -> pushcclosure -> luaC_init whitebits
    }
    namespace signal {
        const int next = 0x20;
        const int state = 0x28;
    }
    namespace proto {
        const int sizecode = 0x98; // Deserialize -> 3rd newarray call for p->code.
        const int code = 0x18;     // Deserialize -> 3rd newarray call for p->code.
        const int constants = 0x8; // Deserialize -> second new array call
        const int sizek = 0x94;    // Deserialize -> Offset right above proto->k;
        const int nupvalues = 0xa8;// Deserialize -> 4th newarray call after switch statement
        const int protos = 0x10;   // "protos"
        const int sizep = 0xa0;    // "protos" -> if (proto->sizep)
    }
    // -- Instance (from provided dump) --
    namespace Instance {
        const int ChildrenEnd = 0x80;
        const int ChildrenStart = 0x78;
        const int ClassDescriptor = 0x18;
        const int ClassName = 0x8;
        const int Name = 0xb0;
        const int Parent = 0x70;
    }
    namespace TaskScheduler {
        const int JobStart = 0xd0;
        const int JobEnd = 0xd8;
        const int JobName = 0x18;
        const int JobName_external = 0x28;
        const int MaxFps = 0xb8;
    }
    namespace Script {
        const int Identity      = 0x60;
        const int UserData      = 0x70;
        const int IsCoreScript  = 0x168;
        const int RequireBypass = 0x7D9;
    }
    namespace LocalScript {
        const int Bytecode  = 0x198;
        const int Hash      = 0xb0;
        const int HashValue = 0x60;
    }
    namespace ModuleScript {
        const int Bytecode  = 0x140;
        const int Hash      = 0xb0;
        const int HashValue = 0x90;
    }
    namespace ByteCode {
        const int Pointer = 0x10;
        const int Size    = 0x18;
    }
    namespace DataModel {
        const int CreatorId = 0x6c0;
        const int GameId    = 0x6c8;
        const int JobId     = 0x138;
        const int PlaceId   = 0x6d0;
        const int ServerIP  = 0x590;
        const int Workspace = 0x168;
    }
    namespace Workspace {
        const int Gravity = 0x930;
        const int World   = 0x3c0;
    }
    namespace Players {
        const int LocalPlayer = 0x120;
    }
    namespace LocalPlayer {
        const int Character   = 0x350;
        const int DisplayName = 0xb0;
        const int LocaleId    = 0x600;
        const int Team        = 0x258;
        const int TeamColor   = 0x44;
    }
    namespace Team {
        const int TeamColor = 0x4;
    }
    namespace World {
        const int Gravity    = 0x1d0;
        const int WorldSteps = 0x5f0;
    }
    namespace Lighting {
        const int Atmosphere = 0x1e0;
        const int Sky        = 0x1d0;
    }
}
"""

# Build functions section
funcs_out = "\n// --- DUMPED FUNCTIONS (macOS Mach-O, rebased to 0x100000000) ---\n"
for name, addr in sorted(results.items()):
    if addr and addr != idc.BADADDR:
        funcs_out += f"uint64_t {name} = 0x{rebase(addr):x};\n"
    else:
        funcs_out += f"uint64_t {name} = 0x0; // NOT FOUND - requires AOB/manual XREF\n"

output = STATIC_OFFSETS + funcs_out

# Print to IDA output
print(output)

# Write file
out_path = os.path.join(os.path.dirname(idaapi.get_input_file_path()), "roblox_full_dump.hpp")
with open(out_path, "w") as f:
    f.write(output)

print(f"\n[+] Written to: {out_path}")
print("==============================================")
