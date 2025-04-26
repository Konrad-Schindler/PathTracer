#include "vk_initializers.hpp"

VkCommandPoolCreateInfo vkinit::commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) {
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = flags,
        .queueFamilyIndex = queueFamilyIndex
    };
}

VkCommandBufferAllocateInfo vkinit::commandBufferAllocateInfo(VkCommandPool pool, uint32_t count) {
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = count
    };
}

VkFenceCreateInfo vkinit::fenceCreateInfo(VkFenceCreateFlags flags) {
    return {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = flags
    };
}

VkSemaphoreCreateInfo vkinit::semaphoreCreateInfo(VkSemaphoreCreateFlags flags) {
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .flags = flags
    };
}



VkCommandBufferBeginInfo vkinit::commandBufferBeginInfo(VkCommandBufferUsageFlags flags) {
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags,
    };
}


VkImageSubresourceRange vkinit::imageSubresourceRange(VkImageAspectFlags aspectMask) {
    return {
        .aspectMask = aspectMask,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS
    };
}

VkSemaphoreSubmitInfo vkinit::semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore) {
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = semaphore,
        .value = 1,
        .stageMask = stageMask,
    };
}


VkCommandBufferSubmitInfo vkinit::commandBufferSubmitInfo(VkCommandBuffer cmdbuffer) {
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmdbuffer,
    };
}


VkSubmitInfo2 vkinit::submitInfo(VkCommandBufferSubmitInfo* cmdBufferSubmitInfo, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
    VkSemaphoreSubmitInfo* waitSemaphoreInfo) {
    return {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = (waitSemaphoreInfo == nullptr) ? 0u : 1u,
        .pWaitSemaphoreInfos = waitSemaphoreInfo,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = cmdBufferSubmitInfo,
        .signalSemaphoreInfoCount = (signalSemaphoreInfo == nullptr) ? 0u : 1u,
        .pSignalSemaphoreInfos = signalSemaphoreInfo
    };
}

VkImageCreateInfo vkinit::imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent) {
    return {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usageFlags,
    };
}

VkImageViewCreateInfo vkinit::imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
    };
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.layerCount = 1;
    createInfo.subresourceRange.aspectMask = aspectFlags;

    return createInfo;
}