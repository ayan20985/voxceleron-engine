#include "Chunk.h"

Chunk::Chunk(const glm::ivec3& pos) : position(pos) {
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
bool Chunk::shouldRenderFace(int x, int y, int z, int dx, int dy, int dz) const {
    // Check if the adjacent voxel is out of bounds or is air/inactive
    int nx = x + dx;
    int ny = y + dy;
    int nz = z + dz;
    
    if (!isInBounds(nx, ny, nz)) {
        return true;  // Render faces at chunk boundaries
    }
    
    const Voxel& neighbor = getVoxel(nx, ny, nz);
    return !neighbor.isActive;
}

void Chunk::generateMesh() {
    vertexData.clear();
    
    // For each voxel in the chunk
    for (int z = 0; z < CHUNK_SIZE; z++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int x = 0; x < CHUNK_SIZE; x++) {
                const Voxel& voxel = getVoxel(x, y, z);
                if (!voxel.isActive) continue;
                
                // World space position of the voxel
                glm::vec3 pos(
                    x + position.x * CHUNK_SIZE,
                    y + position.y * CHUNK_SIZE,
                    z + position.z * CHUNK_SIZE
                );
                
                // Check each face
                // Front face (Z+)
                if (shouldRenderFace(x, y, z, 0, 0, 1)) {
                    glm::vec3 normal(0.0f, 0.0f, 1.0f);
                    vertexData.insert(vertexData.end(), {
                        // pos                          color                                   normal
                        pos.x, pos.y, pos.z + 1,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y, pos.z + 1,   voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x, pos.y + 1, pos.z + 1,   voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y, pos.z + 1,   voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y + 1, pos.z + 1,voxel.color.r, voxel.color.g, voxel.color.b,     normal.x, normal.y, normal.z,
                        pos.x, pos.y + 1, pos.z + 1,   voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z
                    });
                }
                
                // Back face (Z-)
                if (shouldRenderFace(x, y, z, 0, 0, -1)) {
                    glm::vec3 normal(0.0f, 0.0f, -1.0f);
                    vertexData.insert(vertexData.end(), {
                        pos.x + 1, pos.y, pos.z,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x, pos.y, pos.z,           voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y + 1, pos.z,   voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x, pos.y, pos.z,           voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x, pos.y + 1, pos.z,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y + 1, pos.z,   voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z
                    });
                }
                
                // Right face (X+)
                if (shouldRenderFace(x, y, z, 1, 0, 0)) {
                    glm::vec3 normal(1.0f, 0.0f, 0.0f);
                    vertexData.insert(vertexData.end(), {
                        pos.x + 1, pos.y, pos.z + 1,   voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y, pos.z,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y + 1, pos.z + 1,voxel.color.r, voxel.color.g, voxel.color.b,     normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y, pos.z,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y + 1, pos.z,   voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y + 1, pos.z + 1,voxel.color.r, voxel.color.g, voxel.color.b,     normal.x, normal.y, normal.z
                    });
                }
                
                // Left face (X-)
                if (shouldRenderFace(x, y, z, -1, 0, 0)) {
                    glm::vec3 normal(-1.0f, 0.0f, 0.0f);
                    vertexData.insert(vertexData.end(), {
                        pos.x, pos.y, pos.z,           voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x, pos.y, pos.z + 1,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x, pos.y + 1, pos.z,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x, pos.y, pos.z + 1,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x, pos.y + 1, pos.z + 1,   voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x, pos.y + 1, pos.z,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z
                    });
                }
                
                // Top face (Y+)
                if (shouldRenderFace(x, y, z, 0, 1, 0)) {
                    glm::vec3 normal(0.0f, 1.0f, 0.0f);
                    vertexData.insert(vertexData.end(), {
                        pos.x, pos.y + 1, pos.z + 1,   voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y + 1, pos.z + 1,voxel.color.r, voxel.color.g, voxel.color.b,     normal.x, normal.y, normal.z,
                        pos.x, pos.y + 1, pos.z,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y + 1, pos.z + 1,voxel.color.r, voxel.color.g, voxel.color.b,     normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y + 1, pos.z,   voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x, pos.y + 1, pos.z,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z
                    });
                }
                
                // Bottom face (Y-)
                if (shouldRenderFace(x, y, z, 0, -1, 0)) {
                    glm::vec3 normal(0.0f, -1.0f, 0.0f);
                    vertexData.insert(vertexData.end(), {
                        pos.x, pos.y, pos.z,           voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y, pos.z,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x, pos.y, pos.z + 1,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y, pos.z,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x + 1, pos.y, pos.z + 1,   voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z,
                        pos.x, pos.y, pos.z + 1,       voxel.color.r, voxel.color.g, voxel.color.b,      normal.x, normal.y, normal.z
                    });
                }
            }
        }
    }
} 
