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

// Global pointers and state
static float gTargetSpeed = 16.0f;
static float gAppliedSpeed = 16.0f;
static uintptr_t gBaseAddr = 0;
static GLFWwindow* gWindow = nullptr;

// Helper: retrieve base address of a loaded image by name
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

// Background thread for memory patching
void PatchingThread() {
    while (gBaseAddr == 0) {
        gBaseAddr = get_image_base("RobloxPlayer");
        if (gBaseAddr == 0) gBaseAddr = get_image_base("RobloxPlayerBeta");
        usleep(500000);
    }

    while (true) {
        if (gBaseAddr != 0) {
            uintptr_t* playerPtrAddr = reinterpret_cast<uintptr_t*>(gBaseAddr + Walkspeed::LocalPlayer);
            if (playerPtrAddr != nullptr) {
                uintptr_t playerObjPtr = *playerPtrAddr;
                if (playerObjPtr != 0) {
                    float* walkSpeedPtr   = reinterpret_cast<float*>(playerObjPtr + Walkspeed::WalkSpeed);
                    float* speedCheckPtr  = reinterpret_cast<float*>(playerObjPtr + Walkspeed::SpeedCheck);
                    
                    *walkSpeedPtr  = gAppliedSpeed;
                    *speedCheckPtr = gAppliedSpeed;
                }
            }
        }
        usleep(10000);
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
    ImGui::SetNextWindowSize(ImVec2(400, 280), ImGuiCond_Always);
    ImGui::Begin("Antigravity Hack", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    
    ImGui::Text("Target Speed:");
    ImGui::SliderFloat("##target", &gTargetSpeed, 0.0f, 200.0f, "%.1f");
    
    if (ImGui::Button("SET / APPLY SPEED", ImVec2(-1, 40))) {
        gAppliedSpeed = gTargetSpeed;
    }

    ImGui::Separator();
    
    if (ImGui::Button("Reset to Default (16)", ImVec2(-1, 0))) {
        gTargetSpeed = 16.0f;
        gAppliedSpeed = 16.0f;
    }
    
    ImGui::Separator();
    ImGui::Text("Currently Forced: %.1f", gAppliedSpeed);
    ImGui::TextColored(gAppliedSpeed > 16.0f ? ImVec4(1, 0.5f, 0, 1) : ImVec4(0, 1, 0, 1), 
                       gAppliedSpeed > 16.0f ? "Status: Speed Enabled" : "Status: Normal");
    
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

    // Schedule the next frame, but don't block the main thread
    dispatch_async(dispatch_get_main_queue(), ^{
        RenderFrame();
    });
}

void StartUI() {
    // Delay slightly to let Roblox finish its own window setup
    sleep(5);

    if (!glfwInit()) return;
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_COCOA_MENUBAR, GL_FALSE);
    
    gWindow = glfwCreateWindow(400, 280, "Antigravity Speed Hack", nullptr, nullptr);
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
    
    std::cout << "[Hack] Injected. Main thread loop optimized to prevent gray screen.\n";
}
