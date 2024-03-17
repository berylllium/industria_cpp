#pragma once

#include <cstdint>

#include "container/free_list.hpp"
#include "math/vector3.hpp"
#include "voxel/voxel.hpp"

enum class VoxelOctreeNodeMask : uint16_t
{
    ABSENT_OCTANT   = 0b00,
    OCTANT          = 0b01,
    VOXEL_OCTANT    = 0b10,
    VOXEL           = 0b11
};

struct VoxelOctreeNode
{
    // Either points to a VoxelOctreeNode or a Voxel.
    uint32_t branches[8];
    uint16_t masks;             // 0x00 -> Absence of octant.
                                // 0x01 -> Octant.
                                // 0x10 -> Voxel octant.
                                // 0x11 -> Voxel.

    void set_branch_mask(uint32_t branch_idx, uint16_t mask);
    uint16_t get_branch_mask(uint32_t branch_idx);
};

struct VoxelOctree
{
    uint8_t depth;

    FreeList<VoxelOctreeNode> nodes;

    VoxelOctree() = default;

    static std::optional<VoxelOctree> create(uint8_t depth);

    static uint64_t interleave_octree_coordinate(vector3i pos);
    static uint64_t interleave_octree_coordinate(uint64_t x, uint64_t y, uint64_t z);

    void set_voxel(uint64_t ipos, uint32_t voxel_idx);

    void compress_from_leaf(uint64_t leaf_pos);
};
