#include <iostream>
#include <thread>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <mach-o/dyld.h>
#include <dispatch/dispatch.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "offsets.h"
#include "engine.hpp"
#include "executor.hpp"
#include <stdarg.h>

// --- EXPLOIT DEFS ---
typedef void(*print_t)(int type, const char* text, ...);
typedef int64_t(*deserialize_t)(uintptr_t rl, const char* source, const char* bytecode, int len, int env);
typedef int64_t(*spawn_t)(uintptr_t rl);

static print_t rbx_print = nullptr;
static deserialize_t rbx_deserialize = nullptr;
static spawn_t rbx_spawn = nullptr;
static uintptr_t rbx_thread = 0; // This would typically be captured from the engine

// Simple Console Log levels
enum LogLevel {
    LOG_NORMAL = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3
};

void rbx_log(LogLevel level, const char* fmt, ...) {
    if (!rbx_print) return;
    char buffer[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    rbx_print(static_cast<int>(level), buffer);
}

// Global state
static float gSpeed = 16.0f;
static uintptr_t gBaseAddr = 0;
static GLFWwindow* gWindow = nullptr;

// Found Addresses (Dynamic)
static float* gWalkSpeedAddr = nullptr;
static float* gSpeedCheckAddr = nullptr;
static float* gHealthAddr = nullptr;
static float* gJumpPowerAddr = nullptr;

// Executor state
static char gScriptBuffer[65536] = "-- Antigravity Executor\nprint('Hello from Monaco Editor!')\nwarn(\"Both quotes work!\")";

// Helper: retrieve base address of a loaded image
uintptr_t get_image_base(const char* imageName) {
    const uint32_t imageCount = _dyld_image_count();
    for (uint32_t i = 0; i < imageCount; ++i) {
        const char* name = _dyld_get_image_name(i);
        if (name && strstr(name, imageName)) {
            const struct mach_header* header = _dyld_get_image_header(i);
            return reinterpret_cast<uintptr_t>(header);
        }
    }
    return 0;
}


// Memory Scanner: Finds the unique 16.0 ... 0x1E8 ... 16.0 pattern safely
void ScanMemory() {
    task_t task = mach_task_self();
    vm_address_t address = 0;
    vm_size_t size = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object_name;

    // Iterate through memory regions to find valid, readable pages
    while (vm_region_64(task, &address, &size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&info, &count, &object_name) == KERN_SUCCESS) {
        // Only scan regions that are readable and not reserved for system
        if ((info.protection & VM_PROT_READ) && address >= 0x100000000 && address < 0x200000000) {
            for (uintptr_t addr = address; addr < address + size - 0x1E8; addr += 8) {
                // Check if the current pointer is valid to dereference
                try {
                    float val1 = *(float*)addr;
                    if (val1 == 16.0f) {
                        uintptr_t secondAddr = addr + 0x1E8;
                        float val2 = *(float*)secondAddr;
                        if (val2 == 16.0f) {
                            gWalkSpeedAddr = (float*)addr;
                            gSpeedCheckAddr = (float*)secondAddr;
                            gHealthAddr = (float*)(addr - 0x50);
                            gJumpPowerAddr = (float*)(addr + 0x2C);
                            return;
                        }
                    }
                } catch (...) {
                    continue; // Skip failed reads
                }
            }
        }
        address += size;
    }
}


void PatchingThread() {
    while (gBaseAddr == 0) {
        gBaseAddr = get_image_base("RobloxPlayer");
        if (gBaseAddr == 0) gBaseAddr = get_image_base("RobloxPlayerBeta");
        usleep(500000);
    }

    // Init executor as soon as we have the base address
    executor::init();

    // Attempt to grab the global lua_State from GetGlobalState
    if (!executor::g_lua_state && executor::pfn_GetGlobalState) {
        uintptr_t state = executor::pfn_GetGlobalState();
        if (state) {
            executor::g_lua_state = state;
            snprintf(executor::g_status, sizeof(executor::g_status),
                     "Ready. State: 0x%llX", (unsigned long long)state);
        }
    }

    while (true) {
    // Init executor and start resolving engine pointers
    executor::init();

    while (true) {
        // Poll engine chain: TaskScheduler → DataModel → LocalPlayer
        engine::poll();

        // Capture lua_State — try GetGlobalState first, fall back to scheduler chain
        if (!executor::g_lua_state) {
            if (executor::pfn_GetGlobalState) {
                uintptr_t s = executor::pfn_GetGlobalState();
                if (s) {
                    executor::g_lua_state = s;
                    snprintf(executor::g_status, sizeof(executor::g_status),
                             "Ready. State via GetGlobalState: 0x%llX", (unsigned long long)s);
                }
            }
            // Fallback: derive from WaitingHybridScriptsJob
            if (!executor::g_lua_state) {
                uintptr_t s = engine::get_lua_state_from_scheduler();
                if (s) {
                    executor::g_lua_state = s;
                    snprintf(executor::g_status, sizeof(executor::g_status),
                             "Ready. State via Scheduler: 0x%llX", (unsigned long long)s);
                }
            }
        }

        if (gWalkSpeedAddr == nullptr) {
            ScanMemory();
            usleep(1000000);
            continue;
        }

        if (gWalkSpeedAddr && gSpeedCheckAddr) {
            *gWalkSpeedAddr = gSpeed;
            *gSpeedCheckAddr = gSpeed;
        }

        usleep(3000);
    }
}

// Non-blocking Render Loop
void RenderFrame() {
    if (!gWindow || glfwWindowShouldClose(gWindow)) return;

    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(440, 400), ImGuiCond_Always);
    ImGui::Begin("Antigravity", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    
    if (ImGui::BeginTabBar("MainTabs")) {
        if (ImGui::BeginTabItem("Speed Hack")) {
            ImGui::Text("Adjust Speed (Instant):");
            ImGui::SliderFloat("##target", &gSpeed, 0.0f, 200.0f, "%.1f");
            
            ImGui::Separator();
            
            if (ImGui::Button("Reset to Default (16)", ImVec2(-1, 40))) gSpeed = 16.0f;
            
            ImGui::Separator();
            if (gWalkSpeedAddr) {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "STATUS: FOUND PLAYER!");
                ImGui::Text("WS Addr: 0x%llX", (unsigned long long)gWalkSpeedAddr);
            } else {
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "STATUS: Scanning for Player...");
            }
            
            ImGui::Text("Base: 0x%llX", (unsigned long long)gBaseAddr);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Executor")) {
            // Monaco-like dark editor styling
            ImGui::PushStyleColor(ImGuiCol_FrameBg,   ImVec4(0.10f, 0.10f, 0.10f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,       ImVec4(0.85f, 0.85f, 0.85f, 1.0f));

            ImGui::Text("Script Editor:");
            ImGui::InputTextMultiline("##editor", gScriptBuffer, sizeof(gScriptBuffer),
                                      ImVec2(-1, 160), ImGuiInputTextFlags_AllowTabInput);

            ImGui::PopStyleColor(2);
            ImGui::Separator();

            // ── Execute button ────────────────────────────────────────────────
            bool has_state = executor::g_lua_state != 0;
            if (!has_state) ImGui::BeginDisabled();

            if (ImGui::Button("EXECUTE", ImVec2(-1, 38))) {
                // Run in a background thread so the UI doesn't freeze
                std::string script_copy(gScriptBuffer);
                std::thread([script_copy]() {
                    executor::execute(script_copy);
                }).detach();
            }

            if (!has_state) ImGui::EndDisabled();

            // ── Row 2: Self-test | Clear ───────────────────────────────────────
            if (ImGui::Button("Self-Test", ImVec2(120, 25))) {
                std::thread([]() { executor::self_test(); }).detach();
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear", ImVec2(80, 25))) {
                memset(gScriptBuffer, 0, sizeof(gScriptBuffer));
                snprintf(gScriptBuffer, 4, "-- ");
            }

            ImGui::Separator();

            // ── Status bar ────────────────────────────────────────────────────
            if (executor::g_last_ok) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[OK]  %s", executor::g_status);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "[!!]  %s", executor::g_status);
            }

            // Show whether we have a Lua state yet
            if (has_state) {
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f),
                    "State: 0x%llX", (unsigned long long)executor::g_lua_state);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "State: not captured yet (join a game)");
            }

            ImGui::EndTabItem();
        }

        // ── ENGINE TAB ──────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Engine")) {
            auto addr_line = [](const char* label, uintptr_t addr) {
                if (addr)
                    ImGui::TextColored(ImVec4(0.3f,1,0.3f,1), "%-8s 0x%llX", label, (unsigned long long)addr);
                else
                    ImGui::TextColored(ImVec4(1,0.6f,0.1f,1), "%-8s not found", label);
            };

            addr_line("TS:",     engine::g_taskscheduler);
            addr_line("DM:",     engine::g_datamodel);
            addr_line("LP:",     engine::g_local_player);
            addr_line("Lua:",    executor::g_lua_state);
            addr_line("Base:",   gBaseAddr);

            ImGui::Separator();
            // DMLock live readout
            uintptr_t dml_a = engine::rebase(engine::EngineOffsets::DMLock_State);
            bool dm_locked = dml_a && *(uint8_t*)dml_a;
            ImGui::Text("DMLock: ");
            ImGui::SameLine();
            if (dm_locked)
                ImGui::TextColored(ImVec4(0.3f,1,0.3f,1), "LOCKED (safe to read)");
            else
                ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "unlocked (not in game)");

            ImGui::Separator();
            if (ImGui::Button("Force Re-Resolve", ImVec2(-1, 28))) {
                engine::g_taskscheduler = 0;
                engine::g_datamodel    = 0;
                engine::g_local_player = 0;
                executor::g_lua_state  = 0;
                snprintf(executor::g_status, sizeof(executor::g_status), "Resolving...");
            }
            ImGui::EndTabItem();
        }

        // ── WORKSPACE TAB ────────────────────────────────────────────────────────
        if (ImGui::BeginTabItem("Workspace")) {
            static std::vector<std::string> ws_lines;
            static bool ws_scanning = false;
            static char ws_filter[64] = {};

            // Scan button
            if (ws_scanning) {
                ImGui::BeginDisabled();
                ImGui::Button("Scanning...", ImVec2(-1, 30));
                ImGui::EndDisabled();
            } else {
                if (ImGui::Button("Show Workspace", ImVec2(-1, 30))) {
                    ws_scanning = true;
                    // Run on background thread — UI stays responsive
                    std::thread([]() {
                        auto lines = engine::dump_workspace();
                        // Swap in on the "main" side (single write, no mutex needed
                        // since we only flip ws_scanning after assignment)
                        ws_lines   = std::move(lines);
                        ws_scanning = false;
                    }).detach();
                }
            }

            // Filter box
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##wsfilter", "Filter (e.g. Part, Humanoid)...",
                                     ws_filter, sizeof(ws_filter));
            ImGui::Separator();

            // Scrollable tree view
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
            ImGui::BeginChild("##wstree", ImVec2(-1, -1), false,
                              ImGuiWindowFlags_HorizontalScrollbar);

            std::string filter_str(ws_filter);
            int shown = 0;
            for (const auto& line : ws_lines) {
                if (!filter_str.empty() &&
                    line.find(filter_str) == std::string::npos)
                    continue;

                // Colour-code by depth / type
                bool is_header  = (line.rfind("Workspace @", 0) == 0 ||
                                   line.rfind("--", 0) == 0 ||
                                   line.find("---") != std::string::npos);
                bool is_error   = (line.find("null") != std::string::npos ||
                                   line.find("not resolved") != std::string::npos);

                if (is_error)
                    ImGui::TextColored(ImVec4(1,0.4f,0.4f,1), "%s", line.c_str());
                else if (is_header)
                    ImGui::TextColored(ImVec4(0.6f,0.9f,1,1), "%s", line.c_str());
                else
                    ImGui::TextUnformatted(line.c_str());
                ++shown;
            }

            if (ws_lines.empty())
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1),
                    "Press \"Show Workspace\" to scan.");

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    
    ImGui::End();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(gWindow, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(gWindow);

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(16 * NSEC_PER_MSEC)), dispatch_get_main_queue(), ^{
        RenderFrame();
    });
}

void StartUI() {
    if (!glfwInit()) return;
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_COCOA_MENUBAR, GL_FALSE);
    glfwWindowHint(GLFW_FLOATING, GL_TRUE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GL_TRUE);
    
    gWindow = glfwCreateWindow(400, 350, "Antigravity v1.3.6", nullptr, nullptr);
    if (!gWindow) {
        glfwTerminate();
        return;
    }
    
    glfwMakeContextCurrent(gWindow);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(gWindow, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    // Optional: Load a better font if available
    // ImGui::GetIO().Fonts->AddFontDefault();

    RenderFrame();
}

extern "C" __attribute__((visibility("default"))) void __attribute__((constructor)) InitHack() {
    printf("[Antigravity] Initializing v1.3.6...\n");
    // Wait for the game to stabilize before starting threads (fixes early injection crash)
    std::thread([]() {
        sleep(5);
        std::thread(PatchingThread).detach();
        
        dispatch_async(dispatch_get_main_queue(), ^{
            StartUI();
        });
    }).detach();
}
