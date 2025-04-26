#include "engine.hpp"
#include <cassert>
#include <VkBootstrap.h>

#include "vk_initializers.hpp"
#include "vk_images.hpp"
#include "vk_pipelines.hpp"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

constexpr bool enableValidationLayers = true;
const uint32_t WINDOW_WIDTH = 600;
const uint32_t WINDOW_HEIGHT = 600;

Engine* loadedEngine = nullptr;

Engine& Engine::Get()
{
	return *loadedEngine;
}

void Engine::init()
{
	assert(loadedEngine == nullptr);
	loadedEngine = this;
    
    initWindow();

    initVulkan();

    initAllocator();

    createSwapchain(windowExtent.width, windowExtent.height);

    initCommands();

    initSyncStructures();

    initDescriptors();

    initPipelines();

    initialized = true;
}

void Engine::initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Pathtracer", nullptr, nullptr);
}

void Engine::initVulkan()
{
    vkb::InstanceBuilder builder;

    auto instanceResult = builder.set_app_name("Pathtracer")
        .request_validation_layers(enableValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0)
        .build();

    if (!instanceResult) {
        throw std::runtime_error("failed to create Vulkan instance!");
    }

    vkb::Instance vkbInstance = instanceResult.value();
    instance = vkbInstance.instance;
    debugMessenger = vkbInstance.debug_messenger;

    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));

    VkPhysicalDeviceVulkan13Features features13{ 
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = true,
        .dynamicRendering = true,
    };

    VkPhysicalDeviceVulkan12Features features12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
    };


    vkb::PhysicalDeviceSelector selector{ vkbInstance };
    auto vkbPhysicalDeviceResult = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(surface)
        .select();

    if (!vkbPhysicalDeviceResult) {
        throw std::runtime_error("failed to find suitable GPU!");
    }

    vkb::PhysicalDevice vkbPhysicalDevice = vkbPhysicalDeviceResult.value();
    vkb::DeviceBuilder deviceBuilder{ vkbPhysicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();

    device = vkbDevice.device;
    physicalDevice = vkbPhysicalDevice.physical_device;

    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void Engine::createSwapchain(int width, int height)
{
    vkb::SwapchainBuilder swapchainBuilder{ physicalDevice, device, surface };

    swapchainImageFormat = VK_FORMAT_B8G8R8_UNORM;
    VkSurfaceFormatKHR surfaceFormat = VkSurfaceFormatKHR{ 
        .format = swapchainImageFormat, 
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR 
    };

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .set_desired_format(surfaceFormat)
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build().value();

    swapchainExtent = vkbSwapchain.extent;
    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();

    VkExtent3D drawImageExtent = {
        windowExtent.width,
        windowExtent.height,
        1
    };
    drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
        | VK_IMAGE_USAGE_TRANSFER_DST_BIT
        | VK_IMAGE_USAGE_STORAGE_BIT
        | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimgInfo = vkinit::imageCreateInfo(drawImage.imageFormat, drawImageUsages, drawImageExtent);

    VmaAllocationCreateInfo rimgAllocinfo = {};
    rimgAllocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimgAllocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(allocator, &rimgInfo, &rimgAllocinfo, &drawImage.image, &drawImage.allocation, nullptr);

    VkImageViewCreateInfo rviewInfo = vkinit::imageViewCreateInfo(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(device, &rviewInfo, nullptr, &drawImage.imageView));

    deletionQueue.push([=]() {
        vkDestroyImageView(device, drawImage.imageView, nullptr);
        vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);
    });
}

void Engine::initCommands()
{
    VkCommandPoolCreateInfo createInfo = vkinit::commandPoolCreateInfo(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateCommandPool(device, &createInfo, nullptr, &frames[i].commandPool));

        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::commandBufferAllocateInfo(frames[i].commandPool);

        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].commandBuffer));
    }
}

void Engine::destroySwapchain()
{
    vkDestroySwapchainKHR(device, swapchain, nullptr);

    for (auto imageView : swapchainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
}

void Engine::cleanup()
{
    vkDeviceWaitIdle(device);
    
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        vkDestroyCommandPool(device, frames[i].commandPool, nullptr);

        vkDestroyFence(device, frames[i].renderFence, nullptr);
        vkDestroySemaphore(device, frames[i].renderSemaphor, nullptr);
        vkDestroySemaphore(device, frames[i].swapchainSemaphore, nullptr);

        frames[i].deletionQueue.flush();
    }

    deletionQueue.flush();

    destroySwapchain();
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyDevice(device, nullptr);
    vkb::destroy_debug_utils_messenger(instance, debugMessenger);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();

    loadedEngine = nullptr;
}

void Engine::draw()
{
    VK_CHECK(vkWaitForFences(device, 1, &currentFrame().renderFence, true, 1'000'000'000));
    currentFrame().deletionQueue.flush();
    VK_CHECK(vkResetFences(device, 1, &currentFrame().renderFence));

    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(device, swapchain, 1'000'000'000, currentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex));

    VkCommandBuffer cmdBuffer = currentFrame().commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmdBuffer, 0));
    VkCommandBufferBeginInfo cmdBufferBeginInfo = vkinit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    drawExtent.width = drawImage.imageExtent.width;
    drawExtent.height = drawImage.imageExtent.height;

    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));
    vkutil::transitionImage(cmdBuffer, drawImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    vkutil::transitionImage(cmdBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    
    drawBackground(cmdBuffer);

    vkutil::transitionImage(cmdBuffer, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transitionImage(cmdBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkutil::copyImagetoImage(cmdBuffer, drawImage.image, swapchainImages[swapchainImageIndex], drawExtent, swapchainExtent);

    vkutil::transitionImage(cmdBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

    VkCommandBufferSubmitInfo cmdBufferInfo = vkinit::commandBufferSubmitInfo(cmdBuffer);
    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, currentFrame().swapchainSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, currentFrame().renderSemaphor);
    VkSubmitInfo2 submitInfo = vkinit::submitInfo(&cmdBufferInfo, &signalInfo, &waitInfo);

    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, currentFrame().renderFence));

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &currentFrame().renderSemaphor,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &swapchainImageIndex,
    };

    VK_CHECK(vkQueuePresentKHR(graphicsQueue, &presentInfo));

    frameNumber++;
}

void Engine::run()
{
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // TODO Stop rendering on window minimizing

        draw();
    }
}

FrameData& Engine::currentFrame()
{
    return frames[frameNumber % FRAME_OVERLAP];
}

void Engine::initSyncStructures()
{
    VkFenceCreateInfo fenceCreateInfo = vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphoreCreateInfo();

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].renderSemaphor));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &frames[i].swapchainSemaphore));
    }
}

void Engine::initAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physicalDevice,
        .device = device,
        .instance = instance,
    };

    vmaCreateAllocator(&allocatorInfo, &allocator);
    deletionQueue.push([&]() {
        vmaDestroyAllocator(allocator);
    });
}

void Engine::initDescriptors()
{
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
    };

    globalDescriptorAllocator.initPool(device, 10, sizes);
    
    DescriptorLayoutBuilder builder;
    builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
    drawImageDescriptors = globalDescriptorAllocator.allocate(device, drawImageDescriptorLayout);

    VkDescriptorImageInfo imgInfo{
        .imageView = drawImage.imageView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkWriteDescriptorSet drawImageWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = drawImageDescriptors,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &imgInfo
    };

    vkUpdateDescriptorSets(device, 1, &drawImageWrite, 0, nullptr);

    deletionQueue.push([&]() {
        globalDescriptorAllocator.destroyPool(device);
        vkDestroyDescriptorSetLayout(device, drawImageDescriptorLayout, nullptr);
    });
}

void Engine::initPipelines()
{
    initBackgroundPipelines();
}

void Engine::initBackgroundPipelines()
{
    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &drawImageDescriptorLayout,
    };

    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

    VkShaderModule computeDrawShader;
    if (!vkutil::loadShaderModule("shaders/shader_comp.spv", device, computeDrawShader)) {
        std::cout << "Error when building the compute shader" << std::endl;
    }

    VkPipelineShaderStageCreateInfo stageInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeDrawShader,
        .pName = "main"
    };

    VkComputePipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stageInfo,
        .layout = pipelineLayout,
    };

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

    vkDestroyShaderModule(device, computeDrawShader, nullptr);
    deletionQueue.push([&]() {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
    });
}

void Engine::drawBackground(VkCommandBuffer cmdBuffer)
{
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 
        0, 1, &drawImageDescriptors, 0, nullptr);

    vkCmdDispatch(cmdBuffer, std::ceil(drawExtent.width / 16.0), std::ceil(drawExtent.height / 16.0), 1);
}
