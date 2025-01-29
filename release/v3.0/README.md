# Voxceleron Engine v3.0

## Changes since v2.79
- Improved shader management system:
  - Automatic shader compilation with dependency tracking
  - Support for both regular and optimized mesh generation shaders
  - Proper handling of shader hot-reloading
- Fixed validation errors:
  - Resolved compute pipeline descriptor binding issues
  - Fixed framebuffer cleanup in SwapChain and Pipeline classes
- Architecture improvements:
  - Better separation of mesh generation and rendering logic
  - Enhanced memory management in graphics and compute pipelines
  - More robust error handling throughout the engine

## File Structure
```
v3.0/
├── VoxceleronEngine.exe
└── shaders/
    ├── basic.frag / .spv        - Basic fragment shader for rendering
    ├── basic.vert / .spv        - Basic vertex shader for rendering
    ├── mesh_generator.comp / .spv - Standard mesh generation compute shader
    └── mesh_generator_optimized.comp / .spv - Optimized mesh generation with greedy meshing
```

## Technical Details
- Shader Improvements:
  - Added greedy meshing support in optimized mesh generator
  - Enhanced LOD transition handling
  - Improved vertex attribute packing
- Performance Optimizations:
  - More efficient mesh generation using compute shaders
  - Better memory usage in graphics pipeline
  - Reduced validation overhead