# Voxceleron Engine v2.76 Release Notes

## Overview
This release focuses on performance improvements, code cleanup, and enhanced stability. Key improvements include better memory management, removal of legacy systems, and fixed type conversion warnings.

## What's New
- Fixed all type conversion warnings for better stability
- Improved memory management in mesh updates
- Removed legacy world_generation.log system
- Enhanced debug information display
- Optimized ImGui initialization

## Technical Details

### Performance Improvements
- Explicit type casting for all float conversions in mouse handling
- Proper uint32_t casting for Vulkan image counts
- Optimized vertex buffer management
- Improved memory handling in mesh generation

### Debug Features
- Real-time performance metrics
  - FPS and UPS counters
  - Memory usage tracking
  - Vertex and face statistics
  - Performance graphs
  - Camera position display

### System Requirements
- Windows 10 or later
- Vulkan-capable GPU with up-to-date drivers
- CMake 3.10 or higher
- Visual Studio 2019 or later with C++17 support
- Vulkan SDK 1.0 or later

### Dependencies
- GLFW 3.3
- GLM
- ImGui
- Vulkan SDK

## Installation

1. Install the Vulkan SDK from [https://vulkan.lunarg.com/](https://vulkan.lunarg.com/)
2. Build from source:
   ```bash
   mkdir build
   cd build
   cmake ..
   cmake --build . --config Release
   ```

## Controls
- WASD: Camera movement
- Mouse: Look around
- Alt: Toggle mouse cursor
- Escape: Exit application

## Known Issues
- Minor non-critical Vulkan validation layer warnings may appear
- Some debug build warnings remain (non-critical)

## Credits
- Vulkan Community
- GLFW Team
- Omar Cornut (ImGui) 