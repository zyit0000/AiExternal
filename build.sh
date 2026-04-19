#!/bin/bash

# Build script for Roblox Speed Hack (macOS)
# Ensure you have cmake and glfw installed (brew install cmake glfw)

echo "Starting build process..."

# Create build directory
mkdir -p build
cd build

# Run CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Compile
make

if [ $? -eq 0 ]; then
    echo "------------------------------------------------"
    echo "SUCCESS: cheat.dylib has been created in the build folder."
    echo "You can now inject it into Roblox."
    echo "------------------------------------------------"
else
    echo "ERROR: Build failed. Please check the logs above."
fi
