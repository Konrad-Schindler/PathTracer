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

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>

constexpr bool enableValidationLayers = false;
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

    initSwapchain();

    initCommands();

    initSyncStructures();

    initDescriptors();

    initPipelines();

    initImgui();

    initDefaultData();

    camera.velocity = glm::vec3(0);
    camera.position = glm::vec3(0, 0, 5);
    camera.pitch = 0;
    camera.yaw = 0;

    std::string filePath = { "..\\..\\assets\\structure.glb" };
    auto file = loadGLTF(this, filePath);

    assert(file.has_value());
    loadedScenes["structure"] = *file;

    initialized = true;
}

static void requestSwapchainResize(GLFWwindow* window, int width, int height) {
    Engine* engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    engine->resizeRequested = true;
}

void Engine::initWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Pathtracer", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, requestSwapchainResize);

    camera.configureGLFW(window);
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

void Engine::initSwapchain()
{
    createSwapchain(windowExtent.width, windowExtent.height);

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

    VmaAllocationCreateInfo rimgAllocinfo{
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };
    vmaCreateImage(allocator, &rimgInfo, &rimgAllocinfo, &drawImage.image, &drawImage.allocation, nullptr);

    VkImageViewCreateInfo rviewInfo = vkinit::imageViewCreateInfo(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(device, &rviewInfo, nullptr, &drawImage.imageView));

    depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    depthImage.imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo dimgInfo = vkinit::imageCreateInfo(depthImage.imageFormat, depthImageUsages, drawImageExtent);
    vmaCreateImage(allocator, &dimgInfo, &rimgAllocinfo, &depthImage.image, &depthImage.allocation, nullptr);

    VkImageViewCreateInfo dviewInfo = vkinit::imageViewCreateInfo(depthImage.imageFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(device, &dviewInfo, nullptr, &depthImage.imageView));

    deletionQueue.push([=]() {
        vkDestroyImageView(device, drawImage.imageView, nullptr);
        vmaDestroyImage(allocator, drawImage.image, drawImage.allocation);

        vkDestroyImageView(device, depthImage.imageView, nullptr);
        vmaDestroyImage(allocator, depthImage.image, depthImage.allocation);
    });
}

void Engine::createSwapchain(int width, int height)
{
    vkb::SwapchainBuilder swapchainBuilder{ physicalDevice, device, surface };

    swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkSurfaceFormatKHR surfaceFormat = VkSurfaceFormatKHR{ 
        .format = VK_FORMAT_B8G8R8A8_UNORM,
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
}

void Engine::resizeSwapchain()
{
    vkDeviceWaitIdle(device);

    destroySwapchain();

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    windowExtent.width = width;
    windowExtent.height = height;

    createSwapchain(windowExtent.width, windowExtent.height);

    resizeRequested = false;
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

    loadedScenes.clear();
    
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        vkDestroyCommandPool(device, frames[i].commandPool, nullptr);

        vkDestroyFence(device, frames[i].renderFence, nullptr);
        vkDestroySemaphore(device, frames[i].renderSemaphor, nullptr);
        vkDestroySemaphore(device, frames[i].swapchainSemaphore, nullptr);

        frames[i].deletionQueue.flush();
    }

    metalRoughMaterial.clearResources(device);

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
    updateScene();

    VK_CHECK(vkWaitForFences(device, 1, &currentFrame().renderFence, true, 1'000'000'000));

    currentFrame().deletionQueue.flush();
    currentFrame().frameDescriptors.clearPools(device);

    VK_CHECK(vkResetFences(device, 1, &currentFrame().renderFence));

    uint32_t swapchainImageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, 1'000'000'000, currentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        resizeRequested = true;
        return;
    }
    VK_CHECK(result);

    VkCommandBuffer cmdBuffer = currentFrame().commandBuffer;
    VK_CHECK(vkResetCommandBuffer(cmdBuffer, 0));
    VkCommandBufferBeginInfo cmdBufferBeginInfo = vkinit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    drawExtent.width = std::min(swapchainExtent.width, drawImage.imageExtent.width) * renderScale;
    drawExtent.height = std::min(swapchainExtent.height, drawImage.imageExtent.height) * renderScale;

    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));
    vkutil::transitionImage(cmdBuffer, drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    drawBackground(cmdBuffer);

    vkutil::transitionImage(cmdBuffer, drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    vkutil::transitionImage(cmdBuffer, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    drawGeometry(cmdBuffer);

    vkutil::transitionImage(cmdBuffer, drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transitionImage(cmdBuffer, swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkutil::copyImagetoImage(cmdBuffer, drawImage.image, swapchainImages[swapchainImageIndex], drawExtent, swapchainExtent);

    vkutil::transitionImage(cmdBuffer, drawImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
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

    result = vkQueuePresentKHR(graphicsQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        resizeRequested = true;
    }
    VK_CHECK(result);
    frameNumber++;
}

void Engine::run()
{
    while (!glfwWindowShouldClose(window)) {
        auto start = std::chrono::system_clock::now();
        
        glfwPollEvents();

        // TODO Stop rendering on window minimizing

        if (resizeRequested) {
            resizeSwapchain();
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::Begin("Stats")) {
            ImGui::Text("frame time %f ms", stats.frametime);
            ImGui::Text("draw time %f ms", stats.meshDrawTime);
            ImGui::Text("update time %f ms", stats.sceneUpdateTime);
            ImGui::Text("triangles %i", stats.triangleCount);
            ImGui::Text("draw calls %i", stats.drawCallCount);
        }
        ImGui::End();

        /*if (ImGui::Begin("background")) {
            ImGui::SliderFloat("Render Scale", &renderScale, 0.3f, 1.f);

            ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

            ImGui::Text("Selected effect:", selected.name);

            ImGui::SliderInt("Effect Index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

            ImGui::InputFloat4("data1", (float*)&selected.data.data1);
            ImGui::InputFloat4("data2", (float*)&selected.data.data2);
            ImGui::InputFloat4("data3", (float*)&selected.data.data3);
            ImGui::InputFloat4("data4", (float*)&selected.data.data4);
        }
        ImGui::End();*/

        ImGui::Render();

        draw();

        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        stats.frametime = elapsed.count() / 1000.f;
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
    std::vector<DescriptorAllocator::PoolSizeRatio> sizes{
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
    };
    globalDescriptorAllocator.init(device, 10, sizes);
    
    {
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        drawImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
        drawImageDescriptors = globalDescriptorAllocator.allocate(device, drawImageDescriptorLayout);
    }

    {
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        gpuSceneDataDescriptorLayout = builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    {
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        singleImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    DescriptorWriter writer;
    writer.writeImage(0, drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.updateSet(device, drawImageDescriptors);

    deletionQueue.push([&]() {
        globalDescriptorAllocator.destroyPools(device);
        vkDestroyDescriptorSetLayout(device, drawImageDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, gpuSceneDataDescriptorLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, singleImageDescriptorLayout, nullptr);
    });

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        std::vector<DescriptorAllocator::PoolSizeRatio> frameSizes{
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        frames[i].frameDescriptors = DescriptorAllocator{};
        frames[i].frameDescriptors.init(device, 1000, frameSizes);

        deletionQueue.push([&, i]() {
            frames[i].frameDescriptors.destroyPools(device);
        });
    }
}

void Engine::initPipelines()
{
    initBackgroundPipelines();

    metalRoughMaterial.buildPipelines(this);
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
    deletionQueue.push([=]() {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyPipeline(device, gradient.pipeline, nullptr);
        vkDestroyPipeline(device, box.pipeline, nullptr);
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
    // Default Textures

    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 1));
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));

    whiteImage = createImage((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    greyImage = createImage((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    blackImage = createImage((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    std::array<uint32_t, 16 * 16> pixels;
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[16 * y + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    missingTextureImage = createImage(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
    };
    vkCreateSampler(device, &samplerInfo, nullptr, &defaultSamplerNearest);

    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(device, &samplerInfo, nullptr, &defaultSamplerLinear);

    deletionQueue.push([&]() {
        vkDestroySampler(device, defaultSamplerNearest, nullptr);
        vkDestroySampler(device, defaultSamplerLinear, nullptr);
        destroyImage(whiteImage);
        destroyImage(greyImage);
        destroyImage(blackImage);
        destroyImage(missingTextureImage);
    });

    // Default Material

    GLTFMetallicRoughness::MaterialResources materialResources{
        .colorImage = whiteImage,
        .colorSampler = defaultSamplerLinear,
        .metalRoughImage = whiteImage,
        .metalRoughSampler = defaultSamplerLinear,
    };

    AllocatedBuffer materialConstants = createBuffer(sizeof(GLTFMetallicRoughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    auto sceneUniformData = (GLTFMetallicRoughness::MaterialConstants*)materialConstants.allocation->GetMappedData();
    sceneUniformData->colorFactors = glm::vec4(1, 1, 1, 1);
    sceneUniformData->metalRoughFactors = glm::vec4(1, 0.5, 0, 0);

    deletionQueue.push([=]() {
        destroyBuffer(materialConstants);
    });

    materialResources.dataBuffer = materialConstants.buffer;
    defaultData = metalRoughMaterial.writeMaterial(device, MaterialPass::Opaque, materialResources, globalDescriptorAllocator);

    for (auto& mesh : testMeshes) {
        std::shared_ptr<MeshNode> newNode = std::make_shared<MeshNode>();
        newNode->mesh = mesh;
        newNode->localTransform = glm::mat4{ 1.f };
        newNode->globalTransform = glm::mat4{ 1.f };

        for (auto& surface : newNode->mesh->surfaces) {
            surface.material = std::make_shared<GLTFMaterial>(defaultData);
        }

        loadedNodes[mesh->name] = std::move(newNode);
    }
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
    stats.drawCallCount = 0;
    stats.triangleCount = 0;
    auto start = std::chrono::system_clock::now();

    AllocatedBuffer gpuSceneDataBuffer = createBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    currentFrame().deletionQueue.push([=]() {
        destroyBuffer(gpuSceneDataBuffer);
    });

    GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
    *sceneUniformData = sceneData;

    VkDescriptorSet globalDescriptor = currentFrame().frameDescriptors.allocate(device, gpuSceneDataDescriptorLayout);

    DescriptorWriter writer;
    writer.writeBuffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.updateSet(device, globalDescriptor);

    VkRenderingAttachmentInfo colorAttachment = vkinit::attachmentInfo(drawImage.imageView, nullptr);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depthAttachmentInfo(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::renderingInfo(drawExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmdBuffer, &renderInfo);

    VkViewport viewport{
        .width = static_cast<float>(drawExtent.width),
        .height = static_cast<float>(drawExtent.height),
        .maxDepth = 1.f,
    };
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor{
        .extent{
            .width = drawExtent.width,
            .height = drawExtent.height,
        }
    };
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    auto draw = [&](const RenderObject& toDraw) {
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, toDraw.material->pipeline->pipeline);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, toDraw.material->pipeline->layout, 0, 1, &globalDescriptor, 0, nullptr);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, toDraw.material->pipeline->layout, 1, 1, &toDraw.material->materialSet, 0, nullptr);

        vkCmdBindIndexBuffer(cmdBuffer, toDraw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        GPUDrawPushConstants pushConstants{
            .worldMatrix = toDraw.transform,
            .vertexBuffer = toDraw.vertexBufferAddress,
        };
        vkCmdPushConstants(cmdBuffer, toDraw.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

        vkCmdDrawIndexed(cmdBuffer, toDraw.indexCount, 1, toDraw.firstIndex, 0, 0);

        stats.drawCallCount++;
        stats.triangleCount += toDraw.indexCount / 3;
    };

    for (const RenderObject& surface : mainDrawContext.opaqueSurfaces) {
        draw(surface);
    }

    for (const RenderObject& surface : mainDrawContext.transparentSurfaces) {
        draw(surface);
    }
    
    vkCmdEndRendering(cmdBuffer);

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.meshDrawTime = elapsed.count() / 1000.f;
}

AllocatedBuffer Engine::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = usage,
    };

    VmaAllocationCreateInfo vmaallocInfo{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = memoryUsage
    };

    AllocatedBuffer buffer;
    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &vmaallocInfo, &buffer.buffer, &buffer.allocation, &buffer.info));
    return buffer;
}

void Engine::destroyBuffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

AllocatedImage Engine::createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage{
        .imageExtent = size,
        .imageFormat = format,
    };

    VkImageCreateInfo imgInfo = vkinit::imageCreateInfo(format, usage, size);
    if (mipmapped) {
        imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height))));
    }

    VmaAllocationCreateInfo allocInfo{
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };

    VK_CHECK(vmaCreateImage(allocator, &imgInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr));

    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT) {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo viewInfo = vkinit::imageViewCreateInfo(format, newImage.image, aspectFlag);
    viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;

    VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage Engine::createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    size_t dataSize = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadBuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    memcpy(uploadBuffer.info.pMappedData, data, dataSize);

    usage = usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    AllocatedImage newImage = createImage(size, format, usage, mipmapped);

    immediateSubmit([&](VkCommandBuffer cmd) {
        vkutil::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy copyRegion{
            .imageSubresource{
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
            },
            .imageExtent = size
        };

        vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        vkutil::transitionImage(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    destroyBuffer(uploadBuffer);

    return newImage;
}

void Engine::destroyImage(const AllocatedImage& image)
{
    vkDestroyImageView(device, image.imageView, nullptr);
    vmaDestroyImage(allocator, image.image, image.allocation);
}

void Engine::updateScene()
{
    auto start = std::chrono::system_clock::now();

    mainDrawContext.opaqueSurfaces.clear();
    mainDrawContext.transparentSurfaces.clear();

    camera.update();

    sceneData.view = camera.viewMatrix();

    float aspectRatio = (float)windowExtent.width / (float)windowExtent.height;
    sceneData.projection = glm::perspectiveZO(glm::radians(70.f), aspectRatio, 10000.f, 0.1f);
    // flip y direction as Vulkan y is down
    sceneData.projection[1][1] *= -1;

    sceneData.viewprojection = sceneData.projection * sceneData.view;

    sceneData.ambientColor = glm::vec4(.1f);
    sceneData.sunlightColor = glm::vec4(1.f);
    sceneData.sunlightDrection = glm::vec4(0, 1, 0.5, 1.f);

    loadedScenes["structure"]->draw(glm::mat4{ 1.f }, mainDrawContext);

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.sceneUpdateTime = elapsed.count() / 1000.f;
}

GPUMeshBuffers Engine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
    
    GPUMeshBuffers newSurface;
    newSurface.vertexBuffer = createBuffer(vertexBufferSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    VkBufferDeviceAddressInfo deviceAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = newSurface.vertexBuffer.buffer,
    };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(device, &deviceAddressInfo);

    newSurface.indexBuffer = createBuffer(indexBufferSize,VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    AllocatedBuffer staging = createBuffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

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

void GLTFMetallicRoughness::buildPipelines(Engine* engine)
{
    VkShaderModule meshVertexShader;
    if (!vkutil::loadShaderModule("shaders/mesh_vert.spv", engine->device, meshVertexShader)) {
        std::cout << "Error building the vertex shader module!" << std::endl;
    };

    VkShaderModule meshFragmentShader;
    if (!vkutil::loadShaderModule("shaders/mesh_frag.spv", engine->device, meshFragmentShader)) {
        std::cout << "Error building the fragment shader module!" << std::endl;
    };

    VkPushConstantRange matrixRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .size = sizeof(GPUDrawPushConstants),
    };

    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    materialLayout = layoutBuilder.build(engine->device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    VkDescriptorSetLayout layouts[] = {
        engine->gpuSceneDataDescriptorLayout,
        materialLayout
    };

    VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipelineLayoutCreateInfo();
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pSetLayouts = layouts;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &matrixRange;

    VkPipelineLayout newLayout;
    VK_CHECK(vkCreatePipelineLayout(engine->device, &layoutInfo, nullptr, &newLayout));

    opaquePipeline.layout = newLayout;
    transparentPipeline.layout = newLayout;

    PipelineBuilder pipelineBuilder;
    pipelineBuilder.setShaders(meshVertexShader, meshFragmentShader);
    pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
    pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipelineBuilder.setMultisamplingNone();
    pipelineBuilder.disableBlending();
    pipelineBuilder.enableDepthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    pipelineBuilder.setColorAttachmentFormat(engine->drawImage.imageFormat);
    pipelineBuilder.setDepthFormat(engine->depthImage.imageFormat);

    pipelineBuilder.pipelineLayout = newLayout;
    opaquePipeline.pipeline = pipelineBuilder.build(engine->device);

    pipelineBuilder.enableBlendingAdditive();
    pipelineBuilder.enableDepthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    
    transparentPipeline.pipeline = pipelineBuilder.build(engine->device);

    vkDestroyShaderModule(engine->device, meshVertexShader, nullptr);
    vkDestroyShaderModule(engine->device, meshFragmentShader, nullptr);
}

void GLTFMetallicRoughness::clearResources(VkDevice device)
{
    vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
    vkDestroyPipelineLayout(device, opaquePipeline.layout, nullptr);

    vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);
    vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
}

MaterialInstance GLTFMetallicRoughness::writeMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocator& descriptorAllocator)
{
    MaterialInstance matData{
        .materialSet = descriptorAllocator.allocate(device, materialLayout),
        .passType = pass
    };
    if (pass == MaterialPass::Transparent) {
        matData.pipeline = &transparentPipeline;
    }
    else {
        matData.pipeline = &opaquePipeline;
    }
    
    writer.clear();
    writer.writeBuffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.writeImage(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.writeImage(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.updateSet(device, matData.materialSet);

    return matData;
}

void MeshNode::draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    glm::mat4 nodeMatrix = topMatrix * globalTransform;

    for (auto& surface : mesh->surfaces) {
        RenderObject renderObject{
            .indexCount = surface.count,
            .firstIndex = surface.startIndex,
            .indexBuffer = mesh->meshBuffers.indexBuffer.buffer,
            .material = &surface.material->data,
            .transform = nodeMatrix,
            .vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress
        };

        if (surface.material->data.passType == MaterialPass::Transparent) {
            ctx.transparentSurfaces.push_back(renderObject);
        }
        else {
            ctx.opaqueSurfaces.push_back(renderObject);
        }
    }

    Node::draw(topMatrix, ctx);
}
