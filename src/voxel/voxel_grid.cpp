#include "voxel/voxel_grid.hpp"

std::optional<VoxelGrid> VoxelGrid::create(uint16_t octree_depth, float leaf_size)
{
    VoxelGrid out;

    out.octree_depth = octree_depth;
    out.leaf_size = leaf_size;

    out.octrees = *FreeList<VoxelOctree>::create();

    return out;
}

void VoxelGrid::set_voxel(vector3i position, uint32_t voxel_index)
{
    vector3i octree_position = position / (1 << octree_depth);
    vector3i inter_octree_position = position % (1 << octree_depth);

    // Interleave position.
    uint64_t ipos = VoxelOctree::interleave_octree_coordinate(inter_octree_position);

    // Get octree.
    // Find octree.
    for (auto& octree_coordinate : octree_coordinates)
    {
        if (octree_coordinate.first == octree_position)
        {
            (*octrees.get(octree_coordinate.second))->set_voxel(ipos, voxel_index);
            return;
        }
    }

    // Create octree.
    uint32_t new_octree_idx = octrees.insert(*VoxelOctree::create(octree_depth));
    octree_coordinates.push_back({position, new_octree_idx});

    (*octrees.get(new_octree_idx))->set_voxel(ipos, voxel_index);
}
