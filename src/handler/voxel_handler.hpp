#pragma once

#include <optional>
#include <string>

#include "voxel/voxel.hpp"

void voxel_handler_register_voxel(std::string id, Voxel voxel);

std::optional<uint32_t> voxel_handler_get_voxel_index(std::string id);
