#ifndef VOXEL_H
#define VOXEL_H

#include <glm/glm.hpp>

enum class VoxelType {
    AIR,
    DIRT,
    GRASS,
    STONE,
    WOOD,
    LEAVES
};

struct Voxel {
    VoxelType type;
    bool isActive;  // Whether the voxel is visible/solid
    glm::vec3 color;  // Base color of the voxel

    Voxel() : type(VoxelType::AIR), isActive(false), color(1.0f) {}
    
    Voxel(VoxelType t, const glm::vec3& c) : type(t), isActive(t != VoxelType::AIR), color(c) {}

    static glm::vec3 getDefaultColor(VoxelType type) {
        switch (type) {
            case VoxelType::DIRT:
                return glm::vec3(0.6f, 0.3f, 0.0f);
            case VoxelType::GRASS:
                return glm::vec3(0.0f, 0.8f, 0.0f);
            case VoxelType::STONE:
                return glm::vec3(0.5f, 0.5f, 0.5f);
            case VoxelType::WOOD:
                return glm::vec3(0.6f, 0.4f, 0.2f);
            case VoxelType::LEAVES:
                return glm::vec3(0.0f, 0.6f, 0.0f);
            default:
                return glm::vec3(1.0f);
        }
    }
};

#endif // VOXEL_H 