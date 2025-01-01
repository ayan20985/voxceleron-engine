# Voxceleron Engine v2.75

A high-performance voxel engine built with Vulkan.

## Changes in v2.75
- Fixed z-fighting issues at chunk boundaries
- Improved rendering performance by eliminating duplicate faces between chunks
- Optimized mesh generation and face culling
- Enhanced chunk border rendering with consistent face handling

## Performance Improvements
- Reduced vertex count by eliminating duplicate faces at chunk edges
- Improved frame rates through optimized face culling
- Better memory usage by reducing redundant geometry

## Requirements
- Windows 10 or later
- Vulkan-capable GPU
- Latest Vulkan drivers
- At least 4GB RAM

## Files Included
- `voxceron.exe` - Main engine executable
- `imgui.lib` - ImGui library for debug interface
- `shaders/` - Directory containing compiled SPIR-V shaders

## Controls
- WASD - Move camera
- Mouse - Look around
- Space/Shift - Move up/down
- ESC - Exit application

## Debug Features
- Real-time FPS and UPS counters
- Memory usage monitoring
- Vertex and face count display
- Performance graphs

## Known Issues
- None in this release

## Notes
- This is a Release build optimized for performance
- Debug logging is disabled in this version
- Performance may vary based on system specifications 