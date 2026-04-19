#include <iostream>
#include <thread>
#include <unistd.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>

// ImGui and GLFW (OpenGL) headers from the cloned repo
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "offsets.h"

// Global speed value controlled by ImGui slider (default 100)
static float gSpeed = 100.0f;

// Helper: retrieve base address of a loaded image by name (Mach-O on macOS)
uintptr_t get_image_base(const char* imageName) {
    const uint32_t imageCount = _dyld_image_count();
    for (uint32_t i = 0; i < imageCount; ++i) {
        const char* name = _dyld_get_image_name(i);
        if (name && strstr(name, imageName)) {
            const struct mach_header* header = _dyld_get_image_header(i);
            return reinterpret_cast<uintptr_t>(header);
        }
    }
    return 0; // Not found
}

// Thread responsible for both memory patching and ImGui rendering
void HackThread() {
    // 1. Locate RobloxPlayer module base address
    uintptr_t baseAddr = get_image_base("RobloxPlayer");
    if (!baseAddr) {
        std::cerr << "[Hack] Unable to locate RobloxPlayer module.\n";
        return;
    }

    // 2. Initialise GLFW + OpenGL context
    if (!glfwInit()) {
        std::cerr << "[Hack] Failed to init GLFW.\n";
        return;
    }
    
    // macOS 10.15 + OpenGL 3.2 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    // We want the window to be visible by default as requested
    glfwWindowHint(GLFW_VISIBLE, GL_TRUE); 
    
    GLFWwindow* window = glfwCreateWindow(400, 200, "Antigravity Speed Hack", nullptr, nullptr);
    if (!window) {
        std::cerr << "[Hack] Failed to create GLFW window.\n";
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // 3. Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    
    // Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        // Memory patching – using offsets from offsets.h
        uintptr_t playerObjPtr = *(uintptr_t*)(baseAddr + Walkspeed::LocalPlayer);
        if (playerObjPtr) {
            float* walkSpeedPtr   = reinterpret_cast<float*>(playerObjPtr + Walkspeed::WalkSpeed);
            float* speedCheckPtr  = reinterpret_cast<float*>(playerObjPtr + Walkspeed::SpeedCheck);
            
            // Apply speed from slider
            *walkSpeedPtr  = gSpeed;
            *speedCheckPtr = gSpeed;
        }

        // ImGui frame
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // UI Layout
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(400, 200));
        ImGui::Begin("Speed Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        
        ImGui::Text("Adjust your walkspeed and check values:");
        ImGui::SliderFloat("Value", &gSpeed, 0.0f, 200.0f);
        
        if (ImGui::Button("Reset to Default (16)")) {
            gSpeed = 16.0f;
        }
        
        ImGui::Separator();
        ImGui::Text("Target: RobloxPlayer");
        ImGui::Text("Base: 0x%llX", (unsigned long long)baseAddr);

        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

        // Keep CPU usage sane
        usleep(5000); 
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
}

// Entry point – automatically starts when the dylib is loaded
extern "C" __attribute__((visibility("default"))) void __attribute__((constructor)) InitHack() {
    std::thread hackThread(HackThread);
    hackThread.detach();
    std::cout << "[Hack] Dylib loaded - UI initialized.\n";
}
