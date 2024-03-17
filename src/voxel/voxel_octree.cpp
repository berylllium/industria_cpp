#include "voxel/voxel_octree.hpp"

static constexpr uint64_t B[] =
{
    0x9249'2492'4924'9249, 0x30C3'0C30'C30C'30C3, 0xF00F'00F0'0F00'F00F,
    0x00FF'0000'FF00'00FF, 0xFFFF'0000'0000'FFFF
};

static constexpr uint64_t S[] = {2, 4, 8, 16, 32};

void VoxelOctreeNode::set_branch_mask(uint32_t branch_idx, uint16_t mask)
{
    masks = masks & ~(((uint16_t) 0b11) << (branch_idx * 2));
    masks = masks | (mask << branch_idx * 2);
}

uint16_t VoxelOctreeNode::get_branch_mask(uint32_t branch_idx)
{
    return (masks >> (branch_idx * 2)) & 0b11;
}

std::optional<VoxelOctree> VoxelOctree::create(uint8_t depth)
{
    VoxelOctree out;

    out.depth = depth;

    out.nodes = *std::move(FreeList<VoxelOctreeNode>::create());

    VoxelOctreeNode root_node {};

    out.nodes.insert(root_node);

    return out;
}

uint64_t VoxelOctree::interleave_octree_coordinate(vector3i pos)
{
    return interleave_octree_coordinate(pos.x, pos.y, pos.z);
}

uint64_t VoxelOctree::interleave_octree_coordinate(uint64_t x, uint64_t y, uint64_t z)
{
    x = (x | (x << S[4])) & B[4];
    x = (x | (x << S[3])) & B[3];
    x = (x | (x << S[2])) & B[2];
    x = (x | (x << S[1])) & B[1];
    x = (x | (x << S[0])) & B[0];

    y = (y | (y << S[4])) & B[4];
    y = (y | (y << S[3])) & B[3];
    y = (y | (y << S[2])) & B[2];
    y = (y | (y << S[1])) & B[1];
    y = (y | (y << S[0])) & B[0];

    z = (z | (z << S[4])) & B[4];
    z = (z | (z << S[3])) & B[3];
    z = (z | (z << S[2])) & B[2];
    z = (z | (z << S[1])) & B[1];
    z = (z | (z << S[0])) & B[0];

    return (x) | (y << 1) | (z << 2);
}

void VoxelOctree::set_voxel(uint64_t ipos, uint32_t voxel_idx)
{
    // Get the root node.
    VoxelOctreeNode* current_node = *nodes.get(0);

    // Step through octree.
    for (int32_t i = depth - 1; i >= 0; i--)
    {
        uint64_t branch_index = (ipos >> (i * 3)) & 0b111;

        // Check branch state.
        switch ((VoxelOctreeNodeMask) current_node->get_branch_mask(branch_index))
        {
        case VoxelOctreeNodeMask::ABSENT_OCTANT:
        {
            if (i == 0)
            {
                // Set leaf.
                current_node->branches[branch_index] = voxel_idx;
                current_node->set_branch_mask(branch_index, (uint16_t) VoxelOctreeNodeMask::VOXEL);
            }
            else
            {
                // Create new octant.
                VoxelOctreeNode new_node {};

                uint32_t node_idx = nodes.insert(new_node);

                current_node->branches[branch_index] = node_idx;
                current_node->set_branch_mask(branch_index, (uint16_t) VoxelOctreeNodeMask::OCTANT);

                current_node = *nodes.get(node_idx);
            }
        } break;
        case VoxelOctreeNodeMask::OCTANT:
        {
            current_node = *nodes.get(current_node->branches[branch_index]);

        } break;
        case VoxelOctreeNodeMask::VOXEL_OCTANT:
        {
            // Split voxel octant to create room for to be added voxel.

            // Check if voxel octant already consists of to be added voxel.
            if (current_node->branches[branch_index] == voxel_idx)
            {
                // Nothing to be done.
                return;
            }

            VoxelOctreeNode new_node {};

            for (uint16_t j = 0; j < 8; j++)
            {
                // Copy over voxel id.
                new_node.branches[j] = current_node->branches[branch_index];
                new_node.set_branch_mask(j, (uint16_t) VoxelOctreeNodeMask::VOXEL_OCTANT);
            }

            uint32_t node_idx = nodes.insert(new_node);

            current_node->branches[branch_index] = node_idx;
            current_node->set_branch_mask(branch_index, (uint16_t) VoxelOctreeNodeMask::OCTANT);

            current_node = *nodes.get(node_idx);
        } break;
        case VoxelOctreeNodeMask::VOXEL:
        {
            current_node->branches[branch_index] = voxel_idx;
        } break;
        }
    }

    compress_from_leaf(ipos);
}

void VoxelOctree::compress_from_leaf(uint64_t leaf_pos)
{
    // TODO:
}
