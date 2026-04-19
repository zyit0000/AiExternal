cmake_minimum_required(VERSION 3.15)
project(RobloxCheat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define paths
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

# Source files
set(SOURCES
    hack.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Headers
include_directories(${IMGUI_DIR} ${IMGUI_DIR}/backends)

# Find GLFW
# Note: On macOS, you might need to install glfw via 'brew install glfw'
find_package(glfw3 CONFIG REQUIRED)

# Create the dynamic library (dylib)
add_library(cheat SHARED ${SOURCES})

# Target properties for macOS 10.15 support
if(APPLE)
    set_target_properties(cheat PROPERTIES 
        MACOSX_RPATH ON
        BUILD_WITH_INSTALL_RPATH ON
        INSTALL_NAME_DIR "@executable_path"
        LINK_FLAGS "-mmacosx-version-min=10.15"
        COMPILE_FLAGS "-mmacosx-version-min=10.15"
    )
    
    # Link macOS Frameworks
    target_link_libraries(cheat
        PRIVATE
        glfw
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()

# Set output name to cheat.dylib
set_target_properties(cheat PROPERTIES 
    OUTPUT_NAME "cheat" 
    PREFIX "" 
    SUFFIX ".dylib"
)
