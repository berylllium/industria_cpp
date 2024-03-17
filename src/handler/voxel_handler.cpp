#include "handler/voxel_handler.hpp"

#include <vector>
#include <unordered_map>

static std::vector<Voxel> voxels;
static std::unordered_map<std::string, uint32_t> voxel_indices;

void voxel_handler_register_voxel(std::string id, Voxel voxel)
{
    if (voxel_indices.contains(id))
    {
        return;
    }

    uint32_t idx = voxels.size();
    voxels.push_back(voxel);

    voxel_indices[id] = idx;
}

std::optional<uint32_t> voxel_handler_get_voxel_index(std::string id)
{
    if (!voxel_indices.contains(id))
    {
        return std::nullopt;
    }

    return voxel_indices[id];
}
