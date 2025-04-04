This repo is old, voxceleron2 is in-progress.

# Voxceleron Engine
Aim to make a voxel engine that is the modernized version of voxlap using C++ and Vulkan. The final aim is to make a game that can have 64,000 voxel render distances with players being 16x4x4 voxels and 512 concurrent players on a single server.

## old photos
![Screenshot 2025-01-01 234856](https://github.com/user-attachments/assets/056ab220-96e3-4757-a055-e4bc1fbfaacd)
![Screenshot 2025-01-01 170329](https://github.com/user-attachments/assets/ff4ada3f-adcd-404b-9dac-cbce6985b949)

## Current Development Structure
The current codebase is organized for maintainability and extensibility while laying the groundwork for future scaling:

### Directory Structure
```
src/engine/
├── core/                           # Core engine systems
│   ├── Engine.h/cpp                # Main engine class (simplified)
│   ├── Window.h/cpp                # Window management
│   └── GameLoop.h/cpp              # Main loop and timing
├── vulkan/                         # Vulkan systems
│   ├── core/
│   │   ├── VulkanContext.h/cpp     # Instance & device management
│   │   ├── SwapChain.h/cpp         # Swap chain management
│   │   └── CommandPool.h/cpp       # Command buffer management
│   ├── memory/
│   │   ├── Allocator.h/cpp         # Memory allocation
│   │   └── Buffer.h/cpp            # Buffer management
│   └── pipeline/
│       ├── Pipeline.h/cpp          # Graphics pipeline
│       └── RenderPass.h/cpp        # Render pass management
├── renderer/                       # High-level rendering
│   ├── Renderer.h/cpp              # Main renderer (simplified)
│   ├── RenderQueue.h/cpp           # Command scheduling
│   └── Mesh.h/cpp                  # Mesh handling
├── voxel/                          # Voxel engine core
│   ├── World.h/cpp                 # World management
│   ├── Chunk.h/cpp                 # Chunk system
│   ├── ChunkManager.h/cpp          # Chunk loading/unloading
│   └── VoxelTypes.h/cpp            # Voxel definitions
├── graphics/                       # Graphics utilities
│   ├── Camera.h/cpp                # Camera system
│   ├── Shader.h/cpp                # Shader management
│   └── Material.h/cpp              # Material system
└── utils/                          # Core utilities
    ├── Logger.h/cpp                # Logging system
    ├── ThreadPool.h/cpp            # Basic thread management
    ├── Profiler.h/cpp              # Performance tracking
    └── ResourceManager.h/cpp       # Resource management
```

### Key Features and Design Decisions

#### 1. Vulkan Integration
```cpp
// vulkan/core/VulkanContext.h
class VulkanContext {
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    QueueFamilyIndices queueFamilies;
    
public:
    void init();
    void cleanup();
    VkDevice getDevice() const { return device; }
};

// vulkan/core/CommandPool.h
class CommandPool {
    VkCommandPool pool;
    std::vector<VkCommandBuffer> buffers;
    
public:
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
};
```

#### 2. Memory Management
```cpp
// vulkan/memory/Allocator.h
class VulkanAllocator {
    struct Allocation {
        VkDeviceMemory memory;
        size_t offset;
        size_t size;
    };
    
    std::vector<VkDeviceMemory> memoryPools;
    
public:
    Allocation allocate(VkMemoryRequirements reqs);
    void free(const Allocation& alloc);
};

// vulkan/memory/Buffer.h
class Buffer {
    VkBuffer buffer;
    VkDeviceMemory memory;
    size_t size;
    
public:
    void create(VkDeviceSize size, VkBufferUsageFlags usage);
    void* map();
    void unmap();
};
```

#### 3. Rendering System
```cpp
// renderer/RenderQueue.h
class RenderQueue {
    struct RenderCommand {
        std::function<void(VkCommandBuffer)> execute;
        float sortKey;
    };
    
    std::vector<RenderCommand> commands;
    
public:
    void submit(const RenderCommand& cmd);
    void sort();
    void execute(VkCommandBuffer cmd);
};
```

#### 4. Voxel System
```cpp
// voxel/Chunk.h
class Chunk {
    static constexpr int CHUNK_SIZE = 16;
    std::array<Voxel, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE> voxels;
    glm::ivec3 position;
    
public:
    Voxel& getVoxel(int x, int y, int z);
    void setVoxel(int x, int y, int z, const Voxel& voxel);
    void generateMesh();
};

// voxel/ChunkManager.h
class ChunkManager {
    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, Vec3Hash> chunks;
    ThreadPool& threadPool;
    
public:
    void updateChunks(const glm::vec3& playerPos);
    void loadChunk(const glm::ivec3& pos);
    void unloadChunk(const glm::ivec3& pos);
};
```

#### 5. Threading
```cpp
// utils/ThreadPool.h
class ThreadPool {
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    
public:
    void enqueue(std::function<void()> task);
    void waitIdle();
};
```

#### 6. Performance
```cpp
// utils/Profiler.h
class Profiler {
    struct Metric {
        std::string name;
        double value;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    CircularBuffer<Metric> metrics;
    
public:
    void beginFrame();
    void endFrame();
    void addMetric(const std::string& name, double value);
};
```

## Future Goal Structure
The planned architecture to support 64,000 voxel render distances and 512 players:

```
src/engine/
├── core/                           # Core engine systems
│   ├── Engine.h/cpp                # Main engine coordinator
│   ├── Window.h/cpp                # Window management
│   └── Config.h/cpp                # Engine configuration
├── vulkan/                         # High-performance Vulkan systems
│   ├── core/
│   │   ├── VulkanContext.h/cpp     # Vulkan instance & device
│   │   ├── SwapChain.h/cpp         # Presentation management
│   │   └── CommandManager.h/cpp    # Command buffer management
│   ├── memory/
│   │   ├── ChunkAllocator.h/cpp    # Specialized chunk memory management
│   │   ├── StreamingAllocator.h/cpp # Streaming memory for distant chunks
│   │   └── GPUMemoryPool.h/cpp     # GPU memory management
│   ├── pipeline/
│   │   ├── ChunkPipeline.h/cpp     # Specialized voxel rendering pipeline
│   │   ├── LODPipeline.h/cpp       # Level of detail pipeline
│   │   └── ComputePipeline.h/cpp   # Compute-based optimizations
│   └── compute/
│       ├── MeshGenerator.h/cpp     # GPU-accelerated mesh generation
│       ├── ChunkCompressor.h/cpp   # Chunk data compression
│       └── OcclusionCuller.h/cpp   # Compute-based occlusion culling
├── voxel/                          # Voxel engine core
│   ├── world/
│   │   ├── World.h/cpp             # World management
│   │   ├── WorldPartition.h/cpp    # World space partitioning
│   │   └── WorldStreamer.h/cpp     # Streaming world management
│   ├── chunk/
│   │   ├── Chunk.h/cpp             # Chunk data structure
│   │   ├── ChunkManager.h/cpp      # Chunk lifecycle management
│   │   ├── ChunkMesh.h/cpp         # Mesh generation
│   │   ├── ChunkLOD.h/cpp          # Level of detail system
│   │   └── ChunkCache.h/cpp        # Chunk data caching
│   ├── streaming/
│   │   ├── StreamingManager.h/cpp  # Manages chunk streaming
│   │   ├── ChunkLoader.h/cpp       # Asynchronous chunk loading
│   │   └── ChunkCompression.h/cpp  # Chunk data compression
│   └── physics/
│       ├── VoxelCollision.h/cpp    # Voxel-based collision
│       └── PlayerPhysics.h/cpp     # Player movement & collision
├── network/                        # Networking for 512 players
│   ├── core/
│   │   ├── NetworkManager.h/cpp    # Network management
│   │   └── Protocol.h/cpp          # Network protocol
│   ├── client/
│   │   ├── ClientNetwork.h/cpp     # Client networking
│   │   └── Prediction.h/cpp        # Client-side prediction
│   └── server/
│       ├── ServerNetwork.h/cpp     # Server networking
│       ├── PlayerManager.h/cpp     # Manage 512 players
│       └── WorldSync.h/cpp         # World state synchronization
├── threading/                      # Advanced threading
│   ├── ThreadPool.h/cpp            # Thread management
│   ├── JobSystem.h/cpp             # Task scheduling
│   ├── WorkStealingQueue.h/cpp     # Work stealing scheduler
│   └── AsyncTasks.h/cpp            # Async task management
└── optimization/                   # Performance optimization
    ├── MemoryOptimizer.h/cpp       # Memory usage optimization
    ├── ChunkOptimizer.h/cpp        # Chunk optimization
    ├── LODManager.h/cpp            # LOD system
    └── Profiler.h/cpp              # Performance profiling
```

### Advanced Features and Design Decisions

#### 1. High-Performance Vulkan Systems
```cpp
// vulkan/compute/MeshGenerator.h
class ComputeMeshGenerator {
    struct ChunkMeshTask {
        glm::ivec3 position;
        int lodLevel;
        VkBuffer voxelData;
        VkBuffer outputMesh;
    };
    
    VkPipeline computePipeline;
    std::vector<VkDescriptorSet> descriptorSets;
    
public:
    void generateMeshes(const std::vector<ChunkMeshTask>& tasks);
    void dispatchCompute(const ChunkMeshTask& task);
};
```

#### 2. Advanced Memory Management
```cpp
// vulkan/memory/ChunkAllocator.h
class ChunkMemoryManager {
    struct MemoryPool {
        VkDeviceMemory memory;
        std::vector<size_t> freeBlocks;
        size_t totalSize;
        size_t usedSize;
    };
    
    std::vector<MemoryPool> pools;
    
public:
    void* allocateChunkMemory(size_t size);
    void freeChunkMemory(void* ptr);
    void defragment();
};
```

#### 3. Chunk System
```cpp
// voxel/chunk/ChunkLOD.h
class ChunkLOD {
    struct LODLevel {
        int resolution;
        float distance;
        bool useCompression;
    };
    
    static constexpr int MAX_LOD_LEVELS = 8;
    std::array<LODLevel, MAX_LOD_LEVELS> lodLevels;
    
public:
    int calculateLOD(float distanceFromPlayer);
    void optimizeMesh(Chunk& chunk, int lodLevel);
};
```

#### 4. Networking Architecture
```cpp
// network/server/PlayerManager.h
class PlayerManager {
    static constexpr size_t MAX_PLAYERS = 512;
    
    struct PlayerState {
        glm::vec3 position;
        glm::vec3 velocity;
        uint32_t chunkRegion;
        std::vector<uint32_t> visiblePlayers;
    };
    
    std::array<PlayerState, MAX_PLAYERS> players;
    OctreePartition<PlayerState*> playerPartition;
    
public:
    void updatePlayer(uint32_t playerId, const PlayerState& state);
    std::vector<uint32_t> getVisiblePlayers(uint32_t playerId);
};
```

#### 5. Advanced Threading
- Work stealing scheduler
- Priority-based task system
- Lock-free data structures
- Thread pool optimization
- Parallel chunk processing

#### 6. Optimization Systems
- Advanced profiling
- Memory tracking
- Performance analysis
- Automated optimization
- Dynamic LOD adjustment

## Development Phases

### Phase 1: Current Implementation
- Basic Vulkan rendering pipeline
- Simple chunk management
- Basic voxel world generation
- Fundamental memory management
- Basic threading support

### Phase 2: Optimization Foundation
- Improved memory allocation
- Enhanced thread pool
- Better resource management
- Optimized render queue
- Basic profiling system

### Phase 3: Scaling Features
- LOD system implementation
- Chunk streaming
- GPU-accelerated mesh generation
- Memory optimization
- Advanced profiling

### Phase 4: Multiplayer Foundation
- Basic networking
- Player management
- World synchronization
- Client prediction

### Phase 5: Final Goals
- 64,000 voxel render distance
- 512 concurrent players
- Advanced LOD and streaming
- Full networking implementation
- Performance optimization

## Implementation Notes

### Current Version
- Focus on clean, maintainable code
- Proper separation of concerns
- Basic thread safety
- Foundation for future optimizations
- Clear upgrade paths

### Future Version
- Heavy optimization focus
- Advanced memory management
- Sophisticated threading
- Network scalability
- Performance-critical systems

## Getting Started
[To be added: Build instructions, dependencies, etc.]
