#pragma once

#include <cstdint>
#include <string>
#include <vector>

bool platform_init(std::string application_name, int32_t x, int32_t y, int32_t width, int32_t height);

void platform_shutdown();

bool platform_poll_messages();

double platform_get_absolute_time();

void platform_sleep(uint64_t ms);

std::vector<const char*> platform_get_required_instance_extensions();
