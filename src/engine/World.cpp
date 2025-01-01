#include "World.h"
#include <glm/gtc/noise.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>

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
    static std::ofstream logFile("logs/world_generation.log", std::ios::app);
    
    try {
        logFile << "Starting terrain generation for chunk at (" 
                << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ")" << std::endl;
        
        Chunk* chunk = getChunk(chunkPos);
        if (!chunk) {
            logFile << "Error: Chunk not found at position (" 
                    << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ")" << std::endl;
            logFile.flush();
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
        
        logFile << "Generating terrain with shifted base coordinates: (" 
                << baseWX << ", " << baseWY << ", " << baseWZ << ")" << std::endl;
        
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
                            logFile << "Processing coordinates - Local: (" << lx << ", " << ly << ", " << lz 
                                   << "), World Noise: (" << wx << ", " << wy << ", " << wz << ")" << std::endl;
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
                                logFile << "Noise values - Base: " << noise1 
                                       << ", Mid: " << noise2 
                                       << ", High: " << noise3 
                                       << ", Final density: " << density << std::endl;
                            }
                        } catch (const std::exception& e) {
                            logFile << "Error calculating noise at (" << wx << ", " << wy << ", " << wz 
                                   << "): " << e.what() << std::endl;
                            logFile << "Individual coordinates - pos1: (" << pos1.x << ", " << pos1.y << ", " << pos1.z 
                                   << "), pos2: (" << pos2.x << ", " << pos2.y << ", " << pos2.z 
                                   << "), pos3: (" << pos3.x << ", " << pos3.y << ", " << pos3.z << ")" << std::endl;
                            continue;
                        }
                        
                        if (density > 0.1f) {
                            VoxelType blockType = (density > 0.3f) ? VoxelType::STONE : VoxelType::GRASS;
                            try {
                                chunk->setVoxel(lx, ly, lz, 
                                    Voxel(blockType, Voxel::getDefaultColor(blockType)));
                                voxelsSet++;
                                
                                if (voxelsSet % 100 == 0) {
                                    logFile << "Successfully set voxel #" << voxelsSet 
                                           << " at (" << lx << ", " << ly << ", " << lz 
                                           << ") with type " << (blockType == VoxelType::STONE ? "STONE" : "GRASS") 
                                           << std::endl;
                                }
                            } catch (const std::exception& e) {
                                logFile << "Error setting voxel at (" << lx << ", " << ly << ", " << lz 
                                       << "): " << e.what() << std::endl;
                            }
                        }
                    } catch (const std::exception& e) {
                        logFile << "Error in main voxel generation loop at (" << lx << ", " << ly << ", " << lz 
                               << "): " << e.what() << std::endl;
                    }
                }
            }
            
            // Periodically flush the log and check chunk validity
            if (lx % 8 == 0) {
                logFile << "Progress: " << (lx * 100 / Chunk::CHUNK_SIZE) << "% complete, "
                       << voxelsSet << " voxels set so far" << std::endl;
                logFile.flush();
                
                // Verify chunk is still valid
                if (!getChunk(chunkPos)) {
                    logFile << "Error: Chunk was deleted during generation" << std::endl;
                    logFile.flush();
                    return;
                }
            }
        }
        
        logFile << "Set " << voxelsSet << " voxels in chunk" << std::endl;
        
        // Verify chunk is still valid before mesh generation
        chunk = getChunk(chunkPos);
        if (!chunk) {
            logFile << "Error: Chunk was deleted before mesh generation" << std::endl;
            logFile.flush();
            return;
        }
        
        try {
            logFile << "Starting mesh generation..." << std::endl;
            logFile.flush();
            
            // Break mesh generation into steps for better error tracking
            logFile << "Step 1: Clearing old mesh data..." << std::endl;
            chunk->clearMesh();
            
            logFile << "Step 2: Building new mesh..." << std::endl;
            size_t initialVertexCount = chunk->getVertexData().size();
            logFile << "Initial vertex buffer size: " << initialVertexCount << std::endl;
            
            chunk->generateMesh();
            
            size_t finalVertexCount = chunk->getVertexData().size();
            logFile << "Final vertex buffer size: " << finalVertexCount << std::endl;
            logFile << "Added " << (finalVertexCount - initialVertexCount) << " vertices to the mesh" << std::endl;
            
            logFile << "Mesh generation complete" << std::endl;
        } catch (const std::exception& e) {
            logFile << "Error generating mesh: " << e.what() << std::endl;
            logFile.flush();
            throw;
        }
        
        logFile << "Completed terrain generation for chunk at (" 
                << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z << ")" << std::endl;
        logFile.flush();
        
    } catch (const std::exception& e) {
        logFile << "Fatal error generating terrain for chunk at (" 
                << chunkPos.x << ", " << chunkPos.y << ", " << chunkPos.z 
                << "): " << e.what() << std::endl;
        logFile.flush();
        throw;
    }
}

void World::updateChunksAroundCamera(const glm::vec3& cameraPos) {
    static const int CHUNKS_PER_UPDATE = 1; // Only generate one chunk per update to avoid memory spikes
    static int currentX = -WORLD_SIZE/2;
    static int currentY = -WORLD_SIZE/2;
    static int currentZ = -WORLD_SIZE/2;
    
    // Create logs directory if it doesn't exist
    std::filesystem::create_directory("logs");
    static std::ofstream logFile("logs/world_generation.log", std::ios::app);
    
    glm::ivec3 cameraChunkPos = worldToChunkPos(
        static_cast<int>(cameraPos.x),
        static_cast<int>(cameraPos.y),
        static_cast<int>(cameraPos.z)
    );
    
    logFile << "Updating chunks around camera at chunk position (" 
            << cameraChunkPos.x << ", " << cameraChunkPos.y << ", " 
            << cameraChunkPos.z << ")" << std::endl;
    
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
                logFile << "Generating chunk at position (" 
                       << currentX << ", " << currentY << ", " << currentZ << ")" << std::endl;
                
                createChunk(chunkPos);
                generateChunkTerrain(chunkPos);
                chunksGenerated++;
                
                logFile << "Successfully generated chunk at position (" 
                       << currentX << ", " << currentY << ", " << currentZ << ")" << std::endl;
            } catch (const std::exception& e) {
                logFile << "Error generating chunk at (" << currentX << ", " << currentY << ", " << currentZ 
                       << "): " << e.what() << std::endl;
            }
        }
        
        currentZ++;
    }
    
    // Reset if we've finished generating all chunks
    if (currentX >= WORLD_SIZE/2) {
        currentX = -WORLD_SIZE/2;
        currentY = -WORLD_SIZE/2;
        currentZ = -WORLD_SIZE/2;
        logFile << "Completed full world generation cycle" << std::endl;
    }
    
    if (chunksGenerated > 0) {
        logFile << "Generated " << chunksGenerated << " chunks this update" << std::endl;
    }
    
    logFile.flush(); // Make sure the log is written immediately
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