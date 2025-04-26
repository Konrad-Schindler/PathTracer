#pragma once 

#include <vulkan/vulkan.h>

namespace vkutil {
	void transitionImage(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
	void copyImagetoImage(VkCommandBuffer cmdBuffer, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
}
