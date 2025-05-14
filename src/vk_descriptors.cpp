#include "vk_descriptors.hpp"

void DescriptorLayoutBuilder::addBinding(uint32_t binding, VkDescriptorType type)
{
	VkDescriptorSetLayoutBinding newBinding{
		.binding = binding,
		.descriptorType = type,
		.descriptorCount = 1,
	};

	bindings.push_back(newBinding);
}

void DescriptorLayoutBuilder::clear()
{
	bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
	for (auto& binding : bindings) {
		binding.stageFlags |= shaderStages;
	}
	VkDescriptorSetLayoutCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = pNext,
		.flags = flags,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data(),
	};

	VkDescriptorSetLayout set;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

	return set;
}

void DescriptorAllocator::init(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios) 
{
	ratios.clear();

	for (PoolSizeRatio ratio : poolRatios) {
		ratios.push_back(ratio);
	}

	VkDescriptorPool newPool = createPool(device, initialSets, poolRatios);

	setsPerPool = initialSets * 1.5;

	readyPools.push_back(newPool);
}

void DescriptorAllocator::clearPools(VkDevice device)
{
	for (VkDescriptorPool pool : readyPools) {
		vkResetDescriptorPool(device, pool, 0);
	}

	for (VkDescriptorPool pool : fullPools) {
		vkResetDescriptorPool(device, pool, 0);
		readyPools.push_back(pool);
	}
	fullPools.clear();
}

void DescriptorAllocator::destroyPools(VkDevice device)
{
	for (VkDescriptorPool pool : readyPools) {
		vkDestroyDescriptorPool(device, pool, nullptr);
	}
	readyPools.clear();

	for (VkDescriptorPool pool : fullPools) {
		vkDestroyDescriptorPool(device, pool, nullptr);
	}
	fullPools.clear();
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext)
{
	VkDescriptorPool poolToUse = nextPool(device);

	VkDescriptorSetAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = pNext,
		.descriptorPool = poolToUse,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout
	};

	VkDescriptorSet descriptorSet;
	VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);

	// because this is only an if statement readyPools being a vector doesnt make any sense? 
	// If there are 2 pools in readyPools the allocation could fail for both?
	
	// No there can only be more pools in the vector if they were placed there by clearPools(), 
	// so they must be empty
	if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
		fullPools.push_back(poolToUse);

		poolToUse = nextPool(device);
		allocInfo.descriptorPool = poolToUse;

		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
	}

	readyPools.push_back(poolToUse);
	return descriptorSet;
}

VkDescriptorPool DescriptorAllocator::nextPool(VkDevice device)
{
	VkDescriptorPool nextPool;
	if (readyPools.size() != 0) {
		nextPool = readyPools.back();
		readyPools.pop_back();
	}
	else {
		nextPool = createPool(device, setsPerPool, ratios);

		// increase future pool size so if a lot of descriptors are 
		// needed it scales properly without needing tons of allocations
		setsPerPool = setsPerPool * 1.5;
		if (setsPerPool > maxSetsPerPool) {
			setsPerPool = maxSetsPerPool;
		}
	}
	return nextPool;
}

VkDescriptorPool DescriptorAllocator::createPool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = static_cast<uint32_t>(ratio.ratio * setCount)
		});
	}

	VkDescriptorPoolCreateInfo poolInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = setCount,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	VkDescriptorPool newPool;
	vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool);
	return newPool;
}

void DescriptorWriter::writeImage(uint32_t binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
{
	VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
		.sampler = sampler,
		.imageView = image,
		.imageLayout = layout,
	});

	VkWriteDescriptorSet write{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = VK_NULL_HANDLE,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = type,
		.pImageInfo = &info,
	};

	writes.push_back(write);
}

void DescriptorWriter::writeBuffer(uint32_t binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
{
	VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
		.buffer = buffer,
		.offset = offset,
		.range = size
	});

	VkWriteDescriptorSet write{
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = VK_NULL_HANDLE,
		.dstBinding = binding,
		.descriptorCount = 1,
		.descriptorType = type,
		.pBufferInfo = &info,
	};

	writes.push_back(write);
}

void DescriptorWriter::clear()
{
	imageInfos.clear();
	bufferInfos.clear();
	writes.clear();
}

void DescriptorWriter::updateSet(VkDevice device, VkDescriptorSet set)
{
	for (VkWriteDescriptorSet& write : writes) {
		write.dstSet = set;
	}

	vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}
