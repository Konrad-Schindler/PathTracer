#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vk_types.hpp"
#include "vk_descriptors.hpp"

struct DeletionQueue {
	std::deque<std::function<void()>> deletors;

	void push(std::function<void()> function) {
		deletors.push_back(function);
	}

	void flush() {
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)();
		}
		deletors.clear();
	}
};

struct FrameData {
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkSemaphore swapchainSemaphore, renderSemaphor;
	VkFence renderFence;
	DeletionQueue deletionQueue;
};

constexpr unsigned int FRAME_OVERLAP = 2;

struct Engine {
	bool initialized = false;
	int frameNumber{ 0 };
	VkExtent2D windowExtent{ 800, 800 };

	GLFWwindow* window;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;
	VkSurfaceKHR surface;

	VkSwapchainKHR swapchain;
	VkFormat swapchainImageFormat;

	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkExtent2D swapchainExtent;

	FrameData frames[FRAME_OVERLAP];

	VkQueue graphicsQueue;
	uint32_t graphicsQueueFamily;

	DeletionQueue deletionQueue;

	VmaAllocator allocator;
	AllocatedImage drawImage;
	VkExtent2D drawExtent;

	DescriptorAllocator globalDescriptorAllocator;
	VkDescriptorSet drawImageDescriptors;
	VkDescriptorSetLayout drawImageDescriptorLayout;

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	static Engine& Get();

	void init();
	void cleanup();
	void draw();
	void run();
	void initCommands();
	FrameData& currentFrame();

private:
	void initWindow();
	void initVulkan();
	void createSwapchain(int width, int height);
	void destroySwapchain();
	void initSyncStructures();
	void initAllocator();
	void initDescriptors();
	void initPipelines();
	void initBackgroundPipelines();
	void drawBackground(VkCommandBuffer cmdBuffer);
};