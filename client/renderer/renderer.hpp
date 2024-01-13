#pragma once

#include <server/math/vector2.hpp>

bool renderer_initialize();

void renderer_shutdown();

vector2ui renderer_get_framebuffer_size();
