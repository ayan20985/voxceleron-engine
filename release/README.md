# Voxceron Engine

A Vulkan-based voxel engine.

## Requirements

- Windows 10 or later
- Vulkan-capable graphics card with up-to-date drivers

## Running the Engine

1. Make sure you have the latest Vulkan runtime installed on your system
2. Extract all files maintaining the directory structure
3. Run `voxceron.exe`

## Controls

- WASD: Move camera
- Mouse: Look around
- Space/Shift: Move up/down
- ESC: Exit

## Directory Structure

```
voxceron/
├── voxceron.exe
└── shaders/
    ├── fragment_shader.frag.spv
    ├── vertex_shader.vert.spv
    └── voxel_compute.comp.spv
```

## Troubleshooting

If the engine fails to start:
1. Verify that your graphics drivers are up to date
2. Ensure all shader files are in the correct location
3. Check that the Vulkan runtime is installed

## Building from Source

See the main repository for build instructions. 