#pragma once

#include "BindlessVk/Buffers/Buffer.hpp"
#include "Framework/Common/Common.hpp"

class StagingPool
{
public:
	StagingPool(
	    u32 count,
	    usize size,
	    bvk::VkContext const *vk_context,
	    bvk::MemoryAllocator const *memory_allocator
	);

	StagingPool() = default;

	StagingPool(StagingPool &&other);
	StagingPool &operator=(StagingPool &&rhs);

	StagingPool(const StagingPool &rhs) = delete;
	StagingPool &operator=(const StagingPool &rhs) = delete;

	~StagingPool();

	auto get_by_index(u32 index)
	{
		assert_true(staging_buffers.size() > index);
		return &staging_buffers[index];
	}

private:
	vec<bvk::Buffer> staging_buffers = {};
};
