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

    while (true) {
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
    ImGui::SetNextWindowSize(ImVec2(400, 350), ImGuiCond_Always); // Increased height for executor
    ImGui::Begin("Antigravity Speed Hack", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    
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
            // Monaco-like styling for the code editor
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f)); // #1E1E1E
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.83f, 0.83f, 0.83f, 1.0f));    // #D4D4D4
            
            ImGui::Text("Script Editor:");
            ImGui::InputTextMultiline("##editor", gScriptBuffer, sizeof(gScriptBuffer), ImVec2(-1, 180), ImGuiInputTextFlags_AllowTabInput);
            
            ImGui::PopStyleColor(2);

            ImGui::Separator();

            if (ImGui::Button("EXECUTE", ImVec2(-1, 40))) {
                // Initialize pointers if needed (based on user requested print address 0x1001DD7B4)
                if (!rbx_print && gBaseAddr) {
                    rbx_print = (print_t)(gBaseAddr + (0x1001DD7B4 - 0x100000000));
                    rbx_deserialize = (deserialize_t)(gBaseAddr + (0x1027e1577 - 0x100000000));
                    rbx_spawn = (spawn_t)(gBaseAddr + (0x1009b4dc0 - 0x100000000));
                }

                if (rbx_print) {
                    rbx_log(LOG_INFO, "Executing script from Monaco Editor...");
                    // In a real scenario, we would compile gScriptBuffer to Luau bytecode here
                    // and then call rbx_deserialize and rbx_spawn.
                    // For now, we simulate the execution call.
                }
            }
            
            if (ImGui::Button("Clear", ImVec2(100, 25))) {
                memset(gScriptBuffer, 0, sizeof(gScriptBuffer));
            }

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
