cmake_minimum_required(VERSION 3.10)
project(Antigravity)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GLFW Build (Universal)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)

add_subdirectory(glfw)

# Include paths
include_directories(.)
include_directories(glfw/include)
include_directories(imgui)

# --- Cheat Target (cheat.dylib) ---
add_library(cheat SHARED hack.cpp)
set_target_properties(cheat PROPERTIES 
    PREFIX ""
    MACOSX_RPATH ON
    BUILD_WITH_INSTALL_RPATH ON
    INSTALL_NAME_DIR "@executable_path"
    LINK_FLAGS "-mmacosx-version-min=10.15"
    OSX_ARCHITECTURES "x86_64;arm64"
)
target_link_libraries(cheat glfw "-framework OpenGL" "-framework Cocoa" "-framework IOKit" "-framework CoreVideo" "-framework CoreFoundation")

# --- Dumper Target (dumper.dylib) ---
add_library(dumper SHARED dumper.cpp)
set_target_properties(dumper PROPERTIES 
    PREFIX ""
    MACOSX_RPATH ON
    BUILD_WITH_INSTALL_RPATH ON
    INSTALL_NAME_DIR "@executable_path"
    LINK_FLAGS "-mmacosx-version-min=10.15"
    OSX_ARCHITECTURES "x86_64;arm64"
)
target_link_libraries(dumper "-framework CoreFoundation")
