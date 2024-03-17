#pragma once

#include <vector>
#include <utility>

#include "voxel/voxel_octree.hpp"

struct VoxelGrid
{
    FreeList<VoxelOctree> octrees;
    std::vector<std::pair<vector3i, uint32_t>> octree_coordinates;

    vector3f position;
    vector3f rotation;

    uint16_t octree_depth;
    float leaf_size;

    VoxelGrid() = default;

    static std::optional<VoxelGrid> create(uint16_t octree_depth, float leaf_size);

    void set_voxel(vector3i position, uint32_t voxel_index);
};
