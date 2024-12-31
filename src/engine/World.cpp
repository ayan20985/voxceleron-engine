#include "World.h"

World::World() {
}

Chunk* World::getChunk(const glm::ivec3& position) {
    auto it = chunks.find(position);
    return it != chunks.end() ? it->second.get() : nullptr;
}

void World::createChunk(const glm::ivec3& position) {
    if (chunks.find(position) == chunks.end()) {
        chunks[position] = std::make_unique<Chunk>(position);
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
    chunk->generateMesh();  // Regenerate mesh when voxel changes
}

void World::generateTestWorld() {
    // Create a simple flat world with some features
    for (int x = -1; x <= 1; x++) {
        for (int z = -1; z <= 1; z++) {
            createChunk(glm::ivec3(x, 0, z));
            
            // Fill bottom half with stone
            for (int lx = 0; lx < Chunk::CHUNK_SIZE; lx++) {
                for (int ly = 0; ly < Chunk::CHUNK_SIZE / 2; ly++) {
                    for (int lz = 0; lz < Chunk::CHUNK_SIZE; lz++) {
                        setVoxel(
                            x * Chunk::CHUNK_SIZE + lx,
                            ly,
                            z * Chunk::CHUNK_SIZE + lz,
                            Voxel(VoxelType::STONE, Voxel::getDefaultColor(VoxelType::STONE))
                        );
                    }
                }
            }
            
            // Add a layer of dirt
            int dirtLayer = Chunk::CHUNK_SIZE / 2;
            for (int lx = 0; lx < Chunk::CHUNK_SIZE; lx++) {
                for (int lz = 0; lz < Chunk::CHUNK_SIZE; lz++) {
                    setVoxel(
                        x * Chunk::CHUNK_SIZE + lx,
                        dirtLayer,
                        z * Chunk::CHUNK_SIZE + lz,
                        Voxel(VoxelType::DIRT, Voxel::getDefaultColor(VoxelType::DIRT))
                    );
                }
            }
            
            // Add grass on top
            int grassLayer = dirtLayer + 1;
            for (int lx = 0; lx < Chunk::CHUNK_SIZE; lx++) {
                for (int lz = 0; lz < Chunk::CHUNK_SIZE; lz++) {
                    setVoxel(
                        x * Chunk::CHUNK_SIZE + lx,
                        grassLayer,
                        z * Chunk::CHUNK_SIZE + lz,
                        Voxel(VoxelType::GRASS, Voxel::getDefaultColor(VoxelType::GRASS))
                    );
                }
            }
        }
    }
    
    // Add some trees
    auto addTree = [this](int x, int z) {
        int baseY = Chunk::CHUNK_SIZE / 2 + 2;  // Just above grass layer
        
        // Tree trunk
        for (int y = 0; y < 5; y++) {
            setVoxel(x, baseY + y, z,
                Voxel(VoxelType::WOOD, Voxel::getDefaultColor(VoxelType::WOOD)));
        }
        
        // Tree leaves
        for (int dx = -2; dx <= 2; dx++) {
            for (int dy = 3; dy <= 6; dy++) {
                for (int dz = -2; dz <= 2; dz++) {
                    if (abs(dx) == 2 && abs(dz) == 2) continue;  // Skip corners
                    setVoxel(x + dx, baseY + dy, z + dz,
                        Voxel(VoxelType::LEAVES, Voxel::getDefaultColor(VoxelType::LEAVES)));
                }
            }
        }
    };
    
    // Add a few trees
    addTree(0, 0);
    addTree(10, 10);
    addTree(-10, -10);
    addTree(10, -10);
    addTree(-10, 10);
} 