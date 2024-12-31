#ifndef WORLD_H
#define WORLD_H

#include "Chunk.h"
#include <unordered_map>
#include <memory>
#include <mutex>

// Hash function for glm::ivec3 to use in unordered_map
namespace std {
    template<>
    struct hash<glm::ivec3> {
        size_t operator()(const glm::ivec3& v) const {
            return ((hash<int>()(v.x) ^
                   (hash<int>()(v.y) << 1)) >> 1) ^
                   (hash<int>()(v.z) << 1);
        }
    };
}

class World {
public:
    World();
    
    // Chunk management
    Chunk* getChunk(const glm::ivec3& position);
    void createChunk(const glm::ivec3& position);
    void removeChunk(const glm::ivec3& position);
    
    // Get all chunks
    const std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>>& getChunks() const { return chunks; }
    
    // Voxel access (world coordinates)
    Voxel& getVoxel(int x, int y, int z);
    void setVoxel(int x, int y, int z, const Voxel& voxel);
    
    // World update
    void update(double deltaTime);
    
    // Convert world coordinates to chunk coordinates
    static glm::ivec3 worldToChunkPos(int x, int y, int z) {
        return glm::ivec3(
            x < 0 ? (x + 1) / Chunk::CHUNK_SIZE - 1 : x / Chunk::CHUNK_SIZE,
            y < 0 ? (y + 1) / Chunk::CHUNK_SIZE - 1 : y / Chunk::CHUNK_SIZE,
            z < 0 ? (z + 1) / Chunk::CHUNK_SIZE - 1 : z / Chunk::CHUNK_SIZE
        );
    }
    
    // Convert world coordinates to local chunk coordinates
    static glm::ivec3 worldToLocalPos(int x, int y, int z) {
        x = x % Chunk::CHUNK_SIZE;
        y = y % Chunk::CHUNK_SIZE;
        z = z % Chunk::CHUNK_SIZE;
        if (x < 0) x += Chunk::CHUNK_SIZE;
        if (y < 0) y += Chunk::CHUNK_SIZE;
        if (z < 0) z += Chunk::CHUNK_SIZE;
        return glm::ivec3(x, y, z);
    }
    
    // Generate test world
    void generateTestWorld();
    
private:
    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>> chunks;
    std::mutex worldMutex;
};

#endif // WORLD_H 