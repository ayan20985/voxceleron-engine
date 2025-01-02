#include "Chunk.h"
#include "World.h"
#include <iostream>
#include <fstream>
#include <string>
#include "Logger.h"

Chunk::Chunk(World* w, const glm::ivec3& pos) : world(w), position(pos) {
    // Initialize all voxels as air
    for (auto& voxel : voxels) {
        voxel = Voxel();
    }
}

Voxel& Chunk::getVoxel(int x, int y, int z) {
    if (!isInBounds(x, y, z)) {
        static Voxel nullVoxel;
        return nullVoxel;
    }
    return voxels[getIndex(x, y, z)];
}

const Voxel& Chunk::getVoxel(int x, int y, int z) const {
    if (!isInBounds(x, y, z)) {
        static const Voxel nullVoxel;
        return nullVoxel;
    }
    return voxels[getIndex(x, y, z)];
}

void Chunk::setVoxel(int x, int y, int z, const Voxel& voxel) {
    if (isInBounds(x, y, z)) {
        voxels[getIndex(x, y, z)] = voxel;
    }
}

const Voxel& Chunk::getWorldVoxel(int wx, int wy, int wz) const {
    // Convert world coordinates to chunk-local coordinates
    int localX = wx - position.x * CHUNK_SIZE;
    int localY = wy - position.y * CHUNK_SIZE;
    int localZ = wz - position.z * CHUNK_SIZE;
    
    // If the coordinates are within this chunk, use local getVoxel
    if (isInBounds(localX, localY, localZ)) {
        return getVoxel(localX, localY, localZ);
    }
    
    // Otherwise, get the voxel from the world
    return world->getVoxel(wx, wy, wz);
}

bool Chunk::shouldRenderFace(int x, int y, int z, int dx, int dy, int dz) const {
    // Get the current voxel
    const Voxel& voxel = getVoxel(x, y, z);
    if (!voxel.isActive) {
        return false;  // Don't render faces for inactive voxels
    }
    
    // Calculate world coordinates for the neighboring voxel
    int wx = position.x * CHUNK_SIZE + x + dx;
    int wy = position.y * CHUNK_SIZE + y + dy;
    int wz = position.z * CHUNK_SIZE + z + dz;
    
    // Get the neighboring voxel (either from this chunk or a neighboring chunk)
    const Voxel& neighbor = getWorldVoxel(wx, wy, wz);
    
    // If the neighbor is active (not air), don't render the face
    if (neighbor.isActive) {
        return false;
    }
    
    // At chunk boundaries, only render if we're the higher coordinate chunk
    bool isChunkBoundary = false;
    if (dx != 0 && (x == 0 || x == CHUNK_SIZE - 1)) isChunkBoundary = true;
    if (dy != 0 && (y == 0 || y == CHUNK_SIZE - 1)) isChunkBoundary = true;
    if (dz != 0 && (z == 0 || z == CHUNK_SIZE - 1)) isChunkBoundary = true;
    
    if (isChunkBoundary) {
        // For positive direction faces (dx/dy/dz > 0), render if we're at the lower edge (0)
        // For negative direction faces (dx/dy/dz < 0), render if we're at the higher edge (CHUNK_SIZE - 1)
        if (dx > 0 && x == 0) return false;
        if (dx < 0 && x == CHUNK_SIZE - 1) return false;
        if (dy > 0 && y == 0) return false;
        if (dy < 0 && y == CHUNK_SIZE - 1) return false;
        if (dz > 0 && z == 0) return false;
        if (dz < 0 && z == CHUNK_SIZE - 1) return false;
    }
    
    return true;
}

void Chunk::clearMesh() {
    size_t oldSize = vertexData.capacity() * sizeof(float);
    vertexData.clear();
    vertexData.shrink_to_fit();
    size_t newSize = vertexData.capacity() * sizeof(float);
    LOG_WORLDGEN("Cleared mesh data. Memory reduced from " + std::to_string(oldSize) + " to " + std::to_string(newSize) + " bytes");
}

void Chunk::generateMesh() {
    try {
        LOG_WORLDGEN("Starting mesh generation...");
        LOG_WORLDGEN("Step 1: Clearing old mesh data...");
        
        size_t oldCapacity = vertexData.capacity() * sizeof(float);
        LOG_WORLDGEN("Initial vertex buffer size: " + std::to_string(oldCapacity));
        
        // For each voxel in the chunk
        for (int z = 0; z < CHUNK_SIZE; z++) {
            for (int y = 0; y < CHUNK_SIZE; y++) {
                for (int x = 0; x < CHUNK_SIZE; x++) {
                    const Voxel& voxel = getVoxel(x, y, z);
                    if (!voxel.isActive) continue;  // Skip inactive voxels
                    
                    // World space position of the voxel
                    float wx = static_cast<float>(position.x * CHUNK_SIZE + x);
                    float wy = static_cast<float>(position.y * CHUNK_SIZE + y);
                    float wz = static_cast<float>(position.z * CHUNK_SIZE + z);
                    
                    size_t initialSize = vertexData.size();
                    
                    try {
                        // Check each face
                        // Front face (positive Z)
                        if (shouldRenderFace(x, y, z, 0, 0, 1)) {
                            try {
                                // Add vertices for front face
                                vertexData.insert(vertexData.end(), {
                                    wx + 0, wy + 0, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 0, 0, 1,
                                    wx + 1, wy + 0, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 0, 0, 1,
                                    wx + 1, wy + 1, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 0, 0, 1,
                                    wx + 0, wy + 0, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 0, 0, 1,
                                    wx + 1, wy + 1, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 0, 0, 1,
                                    wx + 0, wy + 1, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 0, 0, 1
                                });
                            } catch (const std::exception& e) {
                                LOG_ERROR("Error adding front face vertices at (" + 
                                    std::to_string(x) + ", " + 
                                    std::to_string(y) + ", " + 
                                    std::to_string(z) + "): " + e.what());
                                throw;
                            }
                        }
                        
                        // Back face (negative Z)
                        if (shouldRenderFace(x, y, z, 0, 0, -1)) {
                            try {
                                vertexData.insert(vertexData.end(), {
                                    wx + 1, wy + 0, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 0, 0, -1,
                                    wx + 0, wy + 0, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 0, 0, -1,
                                    wx + 0, wy + 1, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 0, 0, -1,
                                    wx + 1, wy + 0, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 0, 0, -1,
                                    wx + 0, wy + 1, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 0, 0, -1,
                                    wx + 1, wy + 1, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 0, 0, -1
                                });
                            } catch (const std::exception& e) {
                                LOG_ERROR("Error adding back face vertices at (" + 
                                    std::to_string(x) + ", " + 
                                    std::to_string(y) + ", " + 
                                    std::to_string(z) + "): " + e.what());
                                throw;
                            }
                        }
                        
                        // Right face (positive X)
                        if (shouldRenderFace(x, y, z, 1, 0, 0)) {
                            try {
                                vertexData.insert(vertexData.end(), {
                                    wx + 1, wy + 0, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 1, 0, 0,
                                    wx + 1, wy + 0, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 1, 0, 0,
                                    wx + 1, wy + 1, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 1, 0, 0,
                                    wx + 1, wy + 0, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 1, 0, 0,
                                    wx + 1, wy + 1, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 1, 0, 0,
                                    wx + 1, wy + 1, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 1, 0, 0
                                });
                            } catch (const std::exception& e) {
                                LOG_ERROR("Error adding right face vertices at (" + 
                                    std::to_string(x) + ", " + 
                                    std::to_string(y) + ", " + 
                                    std::to_string(z) + "): " + e.what());
                                throw;
                            }
                        }
                        
                        // Left face (negative X)
                        if (shouldRenderFace(x, y, z, -1, 0, 0)) {
                            try {
                                vertexData.insert(vertexData.end(), {
                                    wx + 0, wy + 0, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, -1, 0, 0,
                                    wx + 0, wy + 0, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, -1, 0, 0,
                                    wx + 0, wy + 1, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, -1, 0, 0,
                                    wx + 0, wy + 0, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, -1, 0, 0,
                                    wx + 0, wy + 1, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, -1, 0, 0,
                                    wx + 0, wy + 1, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, -1, 0, 0
                                });
                            } catch (const std::exception& e) {
                                LOG_ERROR("Error adding left face vertices at (" + 
                                    std::to_string(x) + ", " + 
                                    std::to_string(y) + ", " + 
                                    std::to_string(z) + "): " + e.what());
                                throw;
                            }
                        }
                        
                        // Top face (positive Y)
                        if (shouldRenderFace(x, y, z, 0, 1, 0)) {
                            try {
                                vertexData.insert(vertexData.end(), {
                                    wx + 0, wy + 1, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 0, 1, 0,
                                    wx + 1, wy + 1, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 0, 1, 0,
                                    wx + 1, wy + 1, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 0, 1, 0,
                                    wx + 0, wy + 1, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 0, 1, 0,
                                    wx + 1, wy + 1, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 0, 1, 0,
                                    wx + 0, wy + 1, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 0, 1, 0
                                });
                            } catch (const std::exception& e) {
                                LOG_ERROR("Error adding top face vertices at (" + 
                                    std::to_string(x) + ", " + 
                                    std::to_string(y) + ", " + 
                                    std::to_string(z) + "): " + e.what());
                                throw;
                            }
                        }
                        
                        // Bottom face (negative Y)
                        if (shouldRenderFace(x, y, z, 0, -1, 0)) {
                            try {
                                vertexData.insert(vertexData.end(), {
                                    wx + 0, wy + 0, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 0, -1, 0,
                                    wx + 1, wy + 0, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 0, -1, 0,
                                    wx + 1, wy + 0, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 0, -1, 0,
                                    wx + 0, wy + 0, wz + 0, voxel.color.r, voxel.color.g, voxel.color.b, 0, -1, 0,
                                    wx + 1, wy + 0, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 0, -1, 0,
                                    wx + 0, wy + 0, wz + 1, voxel.color.r, voxel.color.g, voxel.color.b, 0, -1, 0
                                });
                            } catch (const std::exception& e) {
                                LOG_ERROR("Error adding bottom face vertices at (" + 
                                    std::to_string(x) + ", " + 
                                    std::to_string(y) + ", " + 
                                    std::to_string(z) + "): " + e.what());
                                throw;
                            }
                        }

                        // Log if we added any vertices for this voxel
                        size_t verticesAdded = vertexData.size() - initialSize;
                        if (verticesAdded > 0) {
                            LOG_WORLDGEN("Added " + std::to_string(verticesAdded) + " vertices for voxel at (" 
                                   + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
                        }
                        
                    } catch (const std::exception& e) {
                        LOG_ERROR("Error processing voxel faces at (" + 
                            std::to_string(x) + ", " + 
                            std::to_string(y) + ", " + 
                            std::to_string(z) + "): " + e.what());
                        throw;
                    }
                }
            }
        }
        LOG_WORLDGEN("Final vertex buffer size: " + std::to_string(vertexData.capacity() * sizeof(float)));
        LOG_WORLDGEN("Added " + std::to_string(vertexData.size()) + " vertices to the mesh");
        LOG_WORLDGEN("Mesh generation complete");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error in mesh generation: " + std::string(e.what()));
        throw;
    }
} 
