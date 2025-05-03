#include "engine.hpp"
#include <cassert>
#include <VkBootstrap.h>

#include "vk_initializers.hpp"
#include "vk_images.hpp"
#include "vk_pipelines.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

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

    initImgui();

    initDefaultData();

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

    swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
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

    VK_CHECK(vkCreateCommandPool(device, &createInfo, nullptr, &immediateCommandPool));
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::commandBufferAllocateInfo(immediateCommandPool);

    VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &immediateCmdBuffer));

    deletionQueue.push([=]() {
        vkDestroyCommandPool(device, immediateCommandPool, nullptr);
    });
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

    drawBackground(cmdBuffer);

    vkutil::transitionImage(cmdBuffer, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    
    drawGeometry(cmdBuffer);

    vkutil::transitionImage(cmdBuffer, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transitionImage(cmdBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkutil::copyImagetoImage(cmdBuffer, drawImage.image, swapchainImages[swapchainImageIndex], drawExtent, swapchainExtent);

    vkutil::transitionImage(cmdBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    drawImGui(cmdBuffer, swapchainImageViews[swapchainImageIndex]);

    vkutil::transitionImage(cmdBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("background")) {
            ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

            ImGui::Text("Selected effect:", selected.name);

            ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

            ImGui::InputFloat4("data1", (float*)&selected.data.data1);
            ImGui::InputFloat4("data2", (float*)&selected.data.data2);
            ImGui::InputFloat4("data3", (float*)&selected.data.data3);
            ImGui::InputFloat4("data4", (float*)&selected.data.data4);
        }
        ImGui::End();

        ImGui::Render();

        draw();
    }
}

FrameData& Engine::currentFrame()
{
    return frames[frameNumber % FRAME_OVERLAP];
}

void Engine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(device, 1, &immediateFence));
    VK_CHECK(vkResetCommandBuffer(immediateCmdBuffer, 0));

    VkCommandBuffer cmdBuffer = immediateCmdBuffer;
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBeginInfo));
    function(cmdBuffer);
    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

    VkCommandBufferSubmitInfo cmdInfo = vkinit::commandBufferSubmitInfo(cmdBuffer);
    VkSubmitInfo2 submit = vkinit::submitInfo(&cmdInfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, immediateFence));

    VK_CHECK(vkWaitForFences(device, 1, &immediateFence, true, 9999999999));
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

    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &immediateFence));
    deletionQueue.push([=]() {
        vkDestroyFence(device, immediateFence, nullptr);
    });
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
    initTrianglePipeline();
}

void Engine::initBackgroundPipelines()
{
    VkPushConstantRange pushConstant{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .size = sizeof(ComputePushConstants),
    };

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &drawImageDescriptorLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstant,
    };

    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

    VkShaderModule computeDrawShader;
    if (!vkutil::loadShaderModule("shaders/shader_comp.spv", device, computeDrawShader)) {
        std::cout << "Error when building the compute shader" << std::endl;
    }

    VkShaderModule gradientShader;
    if (!vkutil::loadShaderModule("shaders/gradient_comp.spv", device, gradientShader)) {
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

    ComputeEffect box{
        .name = "boxshader",
        .layout = pipelineLayout,
        .data = {}
    };

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &box.pipeline));

    pipelineInfo.stage.module = gradientShader;

    ComputeEffect gradient{
    .name = "gradient",
    .layout = pipelineLayout,
    .data = {
            .data1 = glm::vec4(1,0,0,1),
            .data2 = glm::vec4(0,0,1,1)
        }
    };

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gradient.pipeline));

    backgroundEffects.push_back(box);
    backgroundEffects.push_back(gradient);

    vkDestroyShaderModule(device, gradientShader, nullptr);
    vkDestroyShaderModule(device, computeDrawShader, nullptr);
    deletionQueue.push([&]() {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
    });
}

void Engine::initImgui()
{
    VkDescriptorPoolSize poolSizes[]{
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes
    };

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &imguiPool));

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window, true);

    VkPipelineRenderingCreateInfoKHR renderingInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchainImageFormat
    };

    ImGui_ImplVulkan_InitInfo initInfo{
        .Instance = instance,
        .PhysicalDevice = physicalDevice,
        .Device = device,
        .Queue = graphicsQueue,
        .DescriptorPool = imguiPool,
        .MinImageCount = 3,
        .ImageCount = 3,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = renderingInfo,
    };

    ImGui_ImplVulkan_Init(&initInfo);
    ImGui_ImplVulkan_CreateFontsTexture();

    deletionQueue.push([=]() {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(device, imguiPool, nullptr);
    });
}

void Engine::initDefaultData()
{
    std::array<Vertex, 4> rectVertices;

    rectVertices[0].position = { 0.5, -0.5, 0 };
    rectVertices[1].position = { 0.5, 0.5, 0 };
    rectVertices[2].position = { -0.5, -0.5, 0 };
    rectVertices[3].position = { -0.5, 0.5, 0 };

    rectVertices[0].color = { 0, 0, 0, 1 };
    rectVertices[1].color = { 0.5, 0.5, 0.5 , 1 };
    rectVertices[2].color = { 1, 0, 0, 1 };
    rectVertices[3].color = { 0, 1, 0, 1 };

    std::array<uint32_t, 6> rectIndices;

    rectIndices[0] = 0;
    rectIndices[1] = 1;
    rectIndices[2] = 2;

    rectIndices[3] = 2;
    rectIndices[4] = 1;
    rectIndices[5] = 3;

    rectangle = uploadMesh(rectIndices, rectVertices);

    deletionQueue.push([&]() {
        destroyBuffer(rectangle.indexBuffer);
        destroyBuffer(rectangle.vertexBuffer);
    });
}

void Engine::drawBackground(VkCommandBuffer cmdBuffer)
{
    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];
    
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 
        0, 1, &drawImageDescriptors, 0, nullptr);

    vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    vkCmdDispatch(cmdBuffer, std::ceil(drawExtent.width / 16.0), std::ceil(drawExtent.height / 16.0), 1);
}

void Engine::drawImGui(VkCommandBuffer cmdBuffer, VkImageView targetImageView)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::renderingInfo(swapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmdBuffer, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);

    vkCmdEndRendering(cmdBuffer);
}

void Engine::drawGeometry(VkCommandBuffer cmdBuffer)
{
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachmentInfo(drawImage.imageView, nullptr);

    VkRenderingInfo renderInfo = vkinit::renderingInfo(drawExtent, &colorAttachment, nullptr);
    vkCmdBeginRendering(cmdBuffer, &renderInfo);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);

    GPUDrawPushConstants pushConstants{
        .worldMatrix = glm::mat4(1.f),
        .vertexBuffer = rectangle.vertexBufferAddress,
    };

    vkCmdPushConstants(cmdBuffer, trianglePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 
        sizeof(GPUDrawPushConstants), &pushConstants);
    vkCmdBindIndexBuffer(cmdBuffer, rectangle.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    VkViewport viewport{
        .width = static_cast<float>(drawExtent.width),
        .height = static_cast<float>(drawExtent.height),
        .maxDepth = 1.f,
    };
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent.width = drawExtent.width;
    scissor.extent.height = drawExtent.height;
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    vkCmdDrawIndexed(cmdBuffer, 6, 1, 0, 0, 0);
    vkCmdEndRendering(cmdBuffer);
}

void Engine::initTrianglePipeline()
{
    VkShaderModule vertexShader;
    if (!vkutil::loadShaderModule("shaders/triangle_vert.spv", device, vertexShader)) {
        std::cout << "Error building triangle vertex shader!" << std::endl;
    }
    VkShaderModule fragmentShader;
    if (!vkutil::loadShaderModule("shaders/triangle_frag.spv", device, fragmentShader)) {
        std::cout << "Error building triangle fragment shader!" << std::endl;
    }

    VkPushConstantRange bufferRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(GPUDrawPushConstants),
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = vkinit::pipelineLayoutCreateInfo();
    pipelineLayoutInfo.pPushConstantRanges = &bufferRange;
    pipelineLayoutInfo.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &trianglePipelineLayout));

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.pipelineLayout = trianglePipelineLayout;
    pipelineBuilder.setShaders(vertexShader, fragmentShader);
    pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.setMultisamplingNone();
    pipelineBuilder.disableBlending();
    pipelineBuilder.disableDepthtest();
    pipelineBuilder.setColorAttachmentFormat(drawImage.imageFormat);
    pipelineBuilder.setDepthFormat(VK_FORMAT_UNDEFINED);

    trianglePipeline = pipelineBuilder.build(device);

    vkDestroyShaderModule(device, vertexShader, nullptr);
    vkDestroyShaderModule(device, fragmentShader, nullptr);

    deletionQueue.push([&]() {
        vkDestroyPipelineLayout(device, trianglePipelineLayout, nullptr);
        vkDestroyPipeline(device, trianglePipeline, nullptr);
    });
}

AllocatedBuffer Engine::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = usage,
    };

    VmaAllocationCreateInfo vmaallocInfo{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = memoryUsage,
    };

    AllocatedBuffer buffer;
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaallocInfo, &buffer.buffer, &buffer.allocation, &buffer.info));
    return buffer;
}

void Engine::destroyBuffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers Engine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
    
    GPUMeshBuffers newSurface;
    newSurface.vertexBuffer = createBuffer(vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    VkBufferDeviceAddressInfo deviceAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = newSurface.vertexBuffer.buffer,
    };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(device, &deviceAddressInfo);

    newSurface.indexBuffer = createBuffer(indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = createBuffer(vertexBufferSize + indexBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();
    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    immediateSubmit([&](VkCommandBuffer cmdBuffer) {
        VkBufferCopy vertexCopy{
            .size = vertexBufferSize
        };

        vkCmdCopyBuffer(cmdBuffer, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{
            .srcOffset = vertexBufferSize,
            .size = indexBufferSize,
        };

        vkCmdCopyBuffer(cmdBuffer, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    destroyBuffer(staging);

    return newSurface;
}