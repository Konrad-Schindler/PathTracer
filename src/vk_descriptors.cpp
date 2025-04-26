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

void DescriptorAllocator::initPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) 
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = static_cast<uint32_t>(ratio.ratio * maxSets)
		});
	}

	VkDescriptorPoolCreateInfo info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets = maxSets,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	vkCreateDescriptorPool(device, &info, nullptr, &pool);
}

void DescriptorAllocator::clearDescriptors(VkDevice device)
{
	vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroyPool(VkDevice device)
{
	vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
	VkDescriptorSetAllocateInfo info{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout
	};

	VkDescriptorSet descriptorSet;
	VK_CHECK(vkAllocateDescriptorSets(device, &info, &descriptorSet));

	return descriptorSet;
}
