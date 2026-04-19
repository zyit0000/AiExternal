#!/bin/bash
# [ignoring loop detection]

# Check for CMake
if ! command -v cmake &> /dev/null
then
    echo "ERROR: 'cmake' could not be found."
    echo "Please install it on your Mac using Homebrew: brew install cmake"
    exit 1
fi

echo "Starting build process for macOS 10.15..."

# Clean and recreate build directory
rm -rf build
mkdir -p build
cd build

# Run CMake with explicit deployment target
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15

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

