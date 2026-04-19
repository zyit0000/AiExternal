#include <iostream>
#include <thread>
#include <unistd.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <dispatch/dispatch.h> // Required for main thread dispatch

// ImGui and GLFW (OpenGL) headers
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "offsets.h"

// Global speed value controlled by ImGui slider (default 100)
static float gSpeed = 100.0f;
static uintptr_t gBaseAddr = 0;

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

// Background thread for memory patching ONLY
void PatchingThread() {
    while (true) {
        if (gBaseAddr) {
            uintptr_t playerObjPtr = *(uintptr_t*)(gBaseAddr + Walkspeed::LocalPlayer);
            if (playerObjPtr) {
                float* walkSpeedPtr   = reinterpret_cast<float*>(playerObjPtr + Walkspeed::WalkSpeed);
                float* speedCheckPtr  = reinterpret_cast<float*>(playerObjPtr + Walkspeed::SpeedCheck);
                
                // Keep values forced to the slider value
                *walkSpeedPtr  = gSpeed;
                *speedCheckPtr = gSpeed;
            }
        }
        usleep(10000); // 10ms loop
    }
}

// UI Setup - This MUST run on the main thread on macOS
void StartUI() {
    if (!glfwInit()) return;
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_COCOA_MENUBAR, GL_FALSE); // Don't mess with Roblox's menu
    
    GLFWwindow* window = glfwCreateWindow(400, 250, "Antigravity UI", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_Always);
        ImGui::Begin("Speed Hack", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        
        ImGui::Text("Adjust Speed (0 - 200):");
        ImGui::SliderFloat("##speed", &gSpeed, 0.0f, 200.0f);
        
        if (ImGui::Button("Reset to Default (16)", ImVec2(-1, 0))) gSpeed = 16.0f;
        
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "Status: Active");
        ImGui::Text("Base: 0x%llX", (unsigned long long)gBaseAddr);
        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

extern "C" __attribute__((visibility("default"))) void __attribute__((constructor)) InitHack() {
    gBaseAddr = get_image_base("RobloxPlayer");
    
    // 1. Start memory patching in the background
    std::thread(PatchingThread).detach();

    // 2. Dispatch UI creation to the main thread to prevent crashing
    dispatch_async(dispatch_get_main_queue(), ^{
        StartUI();
    });
    
    std::cout << "[Hack] Initialized. UI dispatched to main thread.\n";
}

