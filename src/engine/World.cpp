#include "World.h"
#include <glm/gtc/noise.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include "Logger.h"

World::World() {
}

Chunk* World::getChunk(const glm::ivec3& position) {
    auto it = chunks.find(position);
    return it != chunks.end() ? it->second.get() : nullptr;
}

void World::createChunk(const glm::ivec3& position) {
    if (chunks.find(position) == chunks.end()) {
        chunks[position] = std::make_unique<Chunk>(this, position);
    }
}

void World::removeChunk(const glm::ivec3& position) {
    chunks.erase(position);
}

Voxel& World::getVoxel(int x, int y, int z) {
    glm::ivec3 chunkPos = worldToChunkPos(x, y, z);
    glm::ivec3 localPos = worldToLocalPos(x, y, z);
    
    Chunk* chunk = getChunk(chunkPos);
    if (!chunk) {
        static Voxel nullVoxel;
        return nullVoxel;
    }
    
    return chunk->getVoxel(localPos.x, localPos.y, localPos.z);
}

void World::setVoxel(int x, int y, int z, const Voxel& voxel) {
    glm::ivec3 chunkPos = worldToChunkPos(x, y, z);
    glm::ivec3 localPos = worldToLocalPos(x, y, z);
    
    Chunk* chunk = getChunk(chunkPos);
    if (!chunk) {
        createChunk(chunkPos);
        chunk = getChunk(chunkPos);
    }
    
    chunk->setVoxel(localPos.x, localPos.y, localPos.z, voxel);
    // Don't regenerate mesh here anymore
}

void World::generateChunkTerrain(const glm::ivec3& chunkPos) {
    const float NOISE_SCALE = 0.05f;
    
    try {
        // LOG_WORLDGEN("Generating terrain for chunk at (" + 
        //     std::to_string(chunkPos.x) + ", " + 
        //     std::to_string(chunkPos.y) + ", " + 
        //     std::to_string(chunkPos.z) + ")");
        
        Chunk* chunk = getChunk(chunkPos);
        if (!chunk) {
            // LOG_ERROR("Error: Chunk not found at position (" + 
            //     std::to_string(chunkPos.x) + ", " + 
            //     std::to_string(chunkPos.y) + ", " + 
            //     std::to_string(chunkPos.z) + ")");
            return;
        }
        
        // Pre-calculate base world coordinates for this chunk
        float baseWX = static_cast<float>(chunkPos.x * Chunk::CHUNK_SIZE) * NOISE_SCALE;
        float baseWY = static_cast<float>(chunkPos.y * Chunk::CHUNK_SIZE) * NOISE_SCALE;
        float baseWZ = static_cast<float>(chunkPos.z * Chunk::CHUNK_SIZE) * NOISE_SCALE;
        
        // Shift coordinates to avoid floating point precision issues near zero
        const float COORDINATE_SHIFT = 1000.0f;
        baseWX += COORDINATE_SHIFT;
        baseWY += COORDINATE_SHIFT;
        baseWZ += COORDINATE_SHIFT;
        
        // LOG_WORLDGEN("Generating terrain with shifted base coordinates: (" + 
        //     std::to_string(baseWX) + ", " + 
        //     std::to_string(baseWY) + ", " + 
        //     std::to_string(baseWZ) + ")");
        
        // Generate terrain for this chunk
        int voxelsSet = 0;
        for (int lx = 0; lx < Chunk::CHUNK_SIZE; lx++) {
            float wx = baseWX + static_cast<float>(lx) * NOISE_SCALE;
            for (int ly = 0; ly < Chunk::CHUNK_SIZE; ly++) {
                float wy = baseWY + static_cast<float>(ly) * NOISE_SCALE;
                for (int lz = 0; lz < Chunk::CHUNK_SIZE; lz++) {
                    float wz = baseWZ + static_cast<float>(lz) * NOISE_SCALE;
                    
                    try {
                        // Log coordinates before noise calculation
                        if (voxelsSet % 100 == 0) {
                            // LOG_WORLDGEN("Processing coordinates - Local: (" + 
                            //     std::to_string(lx) + ", " + 
                            //     std::to_string(ly) + ", " + 
                            //     std::to_string(lz) + "), World Noise: (" + 
                            //     std::to_string(wx) + ", " + 
                            //     std::to_string(wy) + ", " + 
                            //     std::to_string(wz) + ")");
                        }
                        
                        // Use 3D Perlin noise for terrain generation
                        glm::vec3 pos1(wx, wy, wz);
                        glm::vec3 pos2(wx * 2.0f, wy * 2.0f, wz * 2.0f);
                        glm::vec3 pos3(wx * 4.0f, wy * 4.0f, wz * 4.0f);
                        
                        float density = 0.0f;
                        try {
                            float noise1 = glm::perlin(pos1);
                            float noise2 = glm::perlin(pos2) * 0.5f;
                            float noise3 = glm::perlin(pos3) * 0.25f;
                            density = noise1 + noise2 + noise3;
                            
                            if (voxelsSet % 100 == 0) {
                                // LOG_WORLDGEN("Noise values - Base: " + 
                                //     std::to_string(noise1) + ", Mid: " + 
                                //     std::to_string(noise2) + ", High: " + 
                                //     std::to_string(noise3) + ", Final density: " + 
                                //     std::to_string(density));
                            }
                        } catch (const std::exception & e) {
                            LOG_ERROR("Error calculating noise at (" + 
                                std::to_string(wx) + ", " + 
                                std::to_string(wy) + ", " + 
                                std::to_string(wz) + "): " + e.what());
                            // LOG_WORLDGEN("Individual coordinates - pos1: (" + 
                            //     std::to_string(pos1.x) + ", " + 
                            //     std::to_string(pos1.y) + ", " + 
                            //     std::to_string(pos1.z) + "), pos2: (" + 
                            //     std::to_string(pos2.x) + ", " + 
                            //     std::to_string(pos2.y) + ", " + 
                            //     std::to_string(pos2.z) + "), pos3: (" + 
                            //     std::to_string(pos3.x) + ", " + 
                            //     std::to_string(pos3.y) + ", " + 
                            //     std::to_string(pos3.z) + ")");
                            continue;
                        }
                        
                        if (density > 0.1f) {
                            VoxelType blockType = (density > 0.3f) ? VoxelType::STONE : VoxelType::GRASS;
                            try {
                                chunk->setVoxel(lx, ly, lz, 
                                    Voxel(blockType, Voxel::getDefaultColor(blockType)));
                                voxelsSet++;
                                
                                if (voxelsSet % 100 == 0) {
                                    // LOG_WORLDGEN("Successfully set voxel #" + 
                                    //     std::to_string(voxelsSet) + " at (" + 
                                    //     std::to_string(lx) + ", " + 
                                    //     std::to_string(ly) + ", " + 
                                    //     std::to_string(lz) + ") with type " + 
                                    //     (blockType == VoxelType::STONE ? "STONE" : "GRASS"));
                                }
                            } catch (const std::exception& e) {
                                LOG_ERROR("Error setting voxel at (" + 
                                    std::to_string(lx) + ", " + 
                                    std::to_string(ly) + ", " + 
                                    std::to_string(lz) + "): " + e.what());
                            }
                        }
                    } catch (const std::exception& e) {
                        LOG_ERROR("Error in main voxel generation loop at (" + 
                            std::to_string(lx) + ", " + 
                            std::to_string(ly) + ", " + 
                            std::to_string(lz) + "): " + e.what());
                    }
                }
            }
            
            // Periodically flush the log and check chunk validity
            if (lx % 8 == 0) {
                // LOG_WORLDGEN("Progress: " + 
                //     std::to_string(lx * 100 / Chunk::CHUNK_SIZE) + "% complete, " + 
                //     std::to_string(voxelsSet) + " voxels set so far");
                
                // Verify chunk is still valid
                if (!getChunk(chunkPos)) {
                    // LOG_ERROR("Error: Chunk was deleted during generation");
                    return;
                }
            }
        }
        
        // LOG_WORLDGEN("Set " + std::to_string(voxelsSet) + " voxels in chunk");
        
        // Verify chunk is still valid before mesh generation
        chunk = getChunk(chunkPos);
        if (!chunk) {
            // LOG_ERROR("Error: Chunk was deleted before mesh generation");
            return;
        }
        
        try {
            // LOG_WORLDGEN("Starting mesh generation...");
            
            // Break mesh generation into steps for better error tracking
            // LOG_WORLDGEN("Step 1: Clearing old mesh data...");
            chunk->clearMesh();
            
            // LOG_WORLDGEN("Step 2: Building new mesh...");
            size_t initialVertexCount = chunk->getVertexData().size();
            // LOG_WORLDGEN("Initial vertex buffer size: " + std::to_string(initialVertexCount));
            
            chunk->generateMesh();
            
            size_t finalVertexCount = chunk->getVertexData().size();
            // LOG_WORLDGEN("Final vertex buffer size: " + std::to_string(finalVertexCount));
            // LOG_WORLDGEN("Added " + std::to_string(finalVertexCount - initialVertexCount) + " vertices to the mesh");
            
            // LOG_WORLDGEN("Mesh generation complete");
        } catch (const std::exception& e) {
            LOG_ERROR("Error generating mesh: " + std::string(e.what()));
            throw;
        }
        
        // LOG_WORLDGEN("Completed terrain generation for chunk at (" + 
        //     std::to_string(chunkPos.x) + ", " + 
        //     std::to_string(chunkPos.y) + ", " + 
        //     std::to_string(chunkPos.z) + ")");
        
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error generating terrain for chunk at (" + 
            std::to_string(chunkPos.x) + ", " + 
            std::to_string(chunkPos.y) + ", " + 
            std::to_string(chunkPos.z) + "): " + e.what());
        throw;
    }
}

void World::updateChunksAroundCamera(const glm::vec3& cameraPos) {
    static const int CHUNKS_PER_UPDATE = 1; // Only generate one chunk per update to avoid memory spikes
    static int currentX = -WORLD_SIZE/2;
    static int currentY = -WORLD_SIZE/2;
    static int currentZ = -WORLD_SIZE/2;
    
    glm::ivec3 cameraChunkPos = worldToChunkPos(
        static_cast<int>(cameraPos.x),
        static_cast<int>(cameraPos.y),
        static_cast<int>(cameraPos.z)
    );
    
    // LOG_WORLDGEN("Updating chunks around camera at chunk position (" + 
    //     std::to_string(cameraChunkPos.x) + ", " + 
    //     std::to_string(cameraChunkPos.y) + ", " + 
    //     std::to_string(cameraChunkPos.z) + ")");
    
    // Generate chunks in view that don't exist yet
    int chunksGenerated = 0;
    
    while (chunksGenerated < CHUNKS_PER_UPDATE && currentX < WORLD_SIZE/2) {
        if (currentY >= WORLD_SIZE/2) {
            currentY = -WORLD_SIZE/2;
            currentX++;
            continue;
        }
        
        if (currentZ >= WORLD_SIZE/2) {
            currentZ = -WORLD_SIZE/2;
            currentY++;
            continue;
        }
        
        glm::ivec3 chunkPos(currentX, currentY, currentZ);
        
        // Skip if chunk already exists
        if (!getChunk(chunkPos)) {
            try {
                // LOG_WORLDGEN("Generating chunk at position (" + 
                //     std::to_string(currentX) + ", " + 
                //     std::to_string(currentY) + ", " + 
                //     std::to_string(currentZ) + ")");
                
                createChunk(chunkPos);
                generateChunkTerrain(chunkPos);
                chunksGenerated++;
                
                // LOG_WORLDGEN("Successfully generated chunk at position (" + 
                //     std::to_string(currentX) + ", " + 
                //     std::to_string(currentY) + ", " + 
                //     std::to_string(currentZ) + ")");
            } catch (const std::exception& e) {
                LOG_ERROR("Error generating chunk at (" + 
                    std::to_string(currentX) + ", " + 
                    std::to_string(currentY) + ", " + 
                    std::to_string(currentZ) + "): " + e.what());
            }
        }
        
        currentZ++;
    }
    
    // Reset if we've finished generating all chunks
    if (currentX >= WORLD_SIZE/2) {
        currentX = -WORLD_SIZE/2;
        currentY = -WORLD_SIZE/2;
        currentZ = -WORLD_SIZE/2;
        // LOG_WORLDGEN("Completed full world generation cycle");
    }
    
    if (chunksGenerated > 0) {
        // LOG_WORLDGEN("Generated " + std::to_string(chunksGenerated) + " chunks this update");
    }
}

void World::generateTestWorld() {
    // Initialize the world bounds but don't generate chunks yet
    // World is now ready for dynamic chunk generation
}

void World::update(double deltaTime) {
    if (!camera) return;
    
    std::lock_guard<std::mutex> lock(worldMutex);
    updateChunksAroundCamera(camera->getPosition());
} 