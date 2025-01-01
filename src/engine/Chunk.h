#ifndef CHUNK_H
#define CHUNK_H

#include "Voxel.h"
#include <array>
#include <vector>
#include <glm/glm.hpp>

class World;  // Forward declaration

class Chunk {
public:
    static constexpr int CHUNK_SIZE = 16;  // 16x16x16 voxels per chunk
    
    Chunk(World* world, const glm::ivec3& position = glm::ivec3(0));
    
    // Voxel access
    Voxel& getVoxel(int x, int y, int z);
    const Voxel& getVoxel(int x, int y, int z) const;
    void setVoxel(int x, int y, int z, const Voxel& voxel);
    
    // Check if coordinates are within chunk bounds
    bool isInBounds(int x, int y, int z) const {
        return x >= 0 && x < CHUNK_SIZE &&
               y >= 0 && y < CHUNK_SIZE &&
               z >= 0 && z < CHUNK_SIZE;
    }
    
    // Get chunk position in world space
    const glm::ivec3& getPosition() const { return position; }
    
    // Generate mesh data for rendering
    void generateMesh();
    void clearMesh();
    const std::vector<float>& getVertexData() const { return vertexData; }
    
private:
    World* world;  // Pointer to the world this chunk belongs to
    glm::ivec3 position;  // Chunk position in world space
    std::array<Voxel, CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE> voxels;
    std::vector<float> vertexData;  // Vertex data for rendering
    
    // Helper function to get voxel index from coordinates
    int getIndex(int x, int y, int z) const {
        return x + CHUNK_SIZE * (y + CHUNK_SIZE * z);
    }
    
    // Check if a voxel face should be rendered
    bool shouldRenderFace(int x, int y, int z, int dx, int dy, int dz) const;
    
    // Get voxel from world coordinates (including neighboring chunks)
    const Voxel& getWorldVoxel(int wx, int wy, int wz) const;
};

#endif // CHUNK_H 