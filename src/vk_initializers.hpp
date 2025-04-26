#pragma once
#include "vk_types.hpp"

namespace vkinit {
	VkCommandPoolCreateInfo commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
	VkCommandBufferAllocateInfo commandBufferAllocateInfo(VkCommandPool pool, uint32_t count = 1);
	VkCommandBufferBeginInfo commandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0);

	VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);
	VkSemaphoreCreateInfo semaphoreCreateInfo(VkSemaphoreCreateFlags flags = 0);

	VkSemaphoreSubmitInfo semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore);
	VkCommandBufferSubmitInfo commandBufferSubmitInfo(VkCommandBuffer cmdbuffer);
	VkSubmitInfo2 submitInfo(VkCommandBufferSubmitInfo* cmdBufferSubmitInfo, VkSemaphoreSubmitInfo* signalSemaphoreInfo, VkSemaphoreSubmitInfo* waitSemaphoreInfo);

	VkImageSubresourceRange imageSubresourceRange(VkImageAspectFlags aspectMask);

	VkImageCreateInfo imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent);
	VkImageViewCreateInfo imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags usageFlags);
}