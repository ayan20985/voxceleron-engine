#ifndef WORLD_H
#define WORLD_H

#include "Chunk.h"
#include "Camera.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>

// Hash function for glm::ivec3 to use in unordered_map
struct Vec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return ((std::hash<int>()(v.x) ^
               (std::hash<int>()(v.y) << 1)) >> 1) ^
               (std::hash<int>()(v.z) << 1);
    }
};

class World {
public:
    World();
    
    void setCamera(Camera* cam) { camera = cam; }
    void update(double deltaTime);
    void generateTestWorld();
    
    Chunk* getChunk(const glm::ivec3& position);
    void createChunk(const glm::ivec3& position);
    void removeChunk(const glm::ivec3& position);
    
    Voxel& getVoxel(int x, int y, int z);
    void setVoxel(int x, int y, int z, const Voxel& voxel);
    
    // Get all chunks for rendering
    const std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, Vec3Hash>& getChunks() const { return chunks; }
    
    // Convert between world and chunk coordinates
    glm::ivec3 worldToChunkPos(int x, int y, int z) {
        return glm::ivec3(
            (x >= 0 ? x : x - Chunk::CHUNK_SIZE + 1) / Chunk::CHUNK_SIZE,
            (y >= 0 ? y : y - Chunk::CHUNK_SIZE + 1) / Chunk::CHUNK_SIZE,
            (z >= 0 ? z : z - Chunk::CHUNK_SIZE + 1) / Chunk::CHUNK_SIZE
        );
    }
    
    glm::ivec3 worldToLocalPos(int x, int y, int z) {
        glm::ivec3 chunkPos = worldToChunkPos(x, y, z);
        return glm::ivec3(
            x - chunkPos.x * Chunk::CHUNK_SIZE,
            y - chunkPos.y * Chunk::CHUNK_SIZE,
            z - chunkPos.z * Chunk::CHUNK_SIZE
        );
    }
    
private:
    static const int WORLD_SIZE = 4;  // World size in chunks (4x4x4)
    static const int WATER_LEVEL = 32;  // Water level in voxels
    
    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, Vec3Hash> chunks;
    Camera* camera;
    std::mutex worldMutex;
    
    // Chunk generation tracking
    int currentX = -WORLD_SIZE/2;
    int currentY = -WORLD_SIZE/2;
    int currentZ = -WORLD_SIZE/2;
    static const int CHUNKS_PER_UPDATE = 2;
    
    void updateChunksAroundCamera(const glm::vec3& cameraPos);
    void generateChunkTerrain(const glm::ivec3& chunkPos);
};

#endif // WORLD_H 