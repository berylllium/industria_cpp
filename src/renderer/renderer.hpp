#pragma once

#include <math/vector2.hpp>

bool renderer_initialize();

void renderer_shutdown();

bool renderer_begin_frame();

bool renderer_end_frame();

vector2ui renderer_get_framebuffer_size();
