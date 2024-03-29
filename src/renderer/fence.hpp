#pragma once

#include "renderer/device.hpp"

struct Fence
{
	vk::Fence handle;
	bool is_signaled;

	const Device* device;

	Fence() = default;

	Fence(const Fence&) = delete; // Prevent copies.

	~Fence();
	
	static std::unique_ptr<Fence> create(const Device* device, bool create_signaled);

	Fence& operator = (const Fence&) = delete; // Prevent copies.

	bool wait(uint64_t timeout_ns = UINT64_MAX);

	bool reset();
};
