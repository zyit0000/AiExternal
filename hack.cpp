#include <iostream>
#include <thread>
#include <unistd.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <dispatch/dispatch.h>

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "offsets.h"

// Global state
static float gSpeed = 16.0f;
static uintptr_t gBaseAddr = 0;
static GLFWwindow* gWindow = nullptr;

// Found Addresses (Dynamic)
static float* gWalkSpeedAddr = nullptr;
static float* gSpeedCheckAddr = nullptr;
static float* gHealthAddr = nullptr;
static float* gJumpPowerAddr = nullptr;

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


// Memory Scanner: Finds the unique 16.0 ... 0x1E8 ... 16.0 pattern
void ScanMemory() {
    uintptr_t start = 0x110000000; 
    uintptr_t end   = 0x160000000; 

    for (uintptr_t addr = start; addr < end; addr += 8) {
        // Look for 16.0f (WalkSpeed)
        if (*(float*)addr == 16.0f) {
            uintptr_t secondAddr = addr + 0x1E8;
            if (secondAddr < end && *(float*)secondAddr == 16.0f) {
                gWalkSpeedAddr = (float*)addr;
                gSpeedCheckAddr = (float*)secondAddr;
                
                // --- INTERNAL DUMPER LOGIC ---
                // Once we find WalkSpeed, we can find others nearby
                // Standard macOS Humanoid offsets:
                // Health is usually -0x50 from WalkSpeed
                // JumpPower is usually +0x2C from WalkSpeed
                gHealthAddr = (float*)(addr - 0x50);
                gJumpPowerAddr = (float*)(addr + 0x2C);
                return;
            }
        }
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
    ImGui::SetNextWindowSize(ImVec2(400, 240), ImGuiCond_Always);
    ImGui::Begin("Antigravity Auto-Scanner", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    
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
    ImGui::End();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(gWindow, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(gWindow);

    dispatch_async(dispatch_get_main_queue(), ^{
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
    
    gWindow = glfwCreateWindow(400, 240, "Antigravity Speed Hack", nullptr, nullptr);
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

    RenderFrame();
}

extern "C" __attribute__((visibility("default"))) void __attribute__((constructor)) InitHack() {
    std::thread(PatchingThread).detach();
    
    dispatch_async(dispatch_get_main_queue(), ^{
        StartUI();
    });
}
