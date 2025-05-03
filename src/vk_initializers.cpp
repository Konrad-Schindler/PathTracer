#include "vk_initializers.hpp"

VkCommandPoolCreateInfo vkinit::commandPoolCreateInfo(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = flags,
        .queueFamilyIndex = queueFamilyIndex
    };
}

VkCommandBufferAllocateInfo vkinit::commandBufferAllocateInfo(VkCommandPool pool, uint32_t count)
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = count
    };
}

VkFenceCreateInfo vkinit::fenceCreateInfo(VkFenceCreateFlags flags)
{
    return {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = flags
    };
}

VkSemaphoreCreateInfo vkinit::semaphoreCreateInfo(VkSemaphoreCreateFlags flags)
{
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .flags = flags
    };
}



VkCommandBufferBeginInfo vkinit::commandBufferBeginInfo(VkCommandBufferUsageFlags flags)
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags,
    };
}


VkImageSubresourceRange vkinit::imageSubresourceRange(VkImageAspectFlags aspectMask)
{
    return {
        .aspectMask = aspectMask,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS
    };
}

VkSemaphoreSubmitInfo vkinit::semaphoreSubmitInfo(VkPipelineStageFlags2 stageMask, VkSemaphore semaphore)
{
    return {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = semaphore,
        .value = 1,
        .stageMask = stageMask,
    };
}


VkCommandBufferSubmitInfo vkinit::commandBufferSubmitInfo(VkCommandBuffer cmdbuffer)
{
    return {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cmdbuffer,
    };
}


VkSubmitInfo2 vkinit::submitInfo(VkCommandBufferSubmitInfo* cmdBufferSubmitInfo, VkSemaphoreSubmitInfo* signalSemaphoreInfo,
    VkSemaphoreSubmitInfo* waitSemaphoreInfo)
{
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

VkImageCreateInfo vkinit::imageCreateInfo(VkFormat format, VkImageUsageFlags usageFlags, VkExtent3D extent)
{
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

VkImageViewCreateInfo vkinit::imageViewCreateInfo(VkFormat format, VkImage image, VkImageAspectFlags aspectFlags)
{
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

VkRenderingAttachmentInfo vkinit::attachmentInfo(VkImageView view, VkClearValue* clear, VkImageLayout layout)
{
    return {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = view,
        .imageLayout = layout,
        .loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear ? *clear : VkClearValue{}
    };
}

VkRenderingInfo vkinit::renderingInfo(VkExtent2D renderExtent, VkRenderingAttachmentInfo* colorAttachment, 
    VkRenderingAttachmentInfo* depthAttachment)
{
    return {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = VkRect2D { VkOffset2D { 0,0 }, renderExtent },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = colorAttachment,
        .pDepthAttachment = depthAttachment,
    };
}

VkPipelineShaderStageCreateInfo vkinit::pipelineShaderStageCreateInfo(VkShaderStageFlagBits stage, VkShaderModule shader,
    const char* entry)
{
    return {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = stage,
        .module = shader,
        .pName = entry
    };
}

VkPipelineLayoutCreateInfo vkinit::pipelineLayoutCreateInfo()
{
    return{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };
}
