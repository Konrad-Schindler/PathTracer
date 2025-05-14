#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vk_types.hpp"
#include "vk_descriptors.hpp"
#include "vk_loader.hpp"
#include "camera.hpp"

struct EngineStats {
	float frametime;
	int triangleCount;
	int drawCallCount;
	float sceneUpdateTime;
	float meshDrawTime;
};

struct MeshNode : public Node {
	std::shared_ptr<MeshAsset> mesh;

	virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct GLTFMetallicRoughness {

	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metalRoughFactors;
		// padding to get 256 bytes (minimum requirement all gpus meet)
		glm::vec4 extra[14];
	};

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	DescriptorWriter writer;

	void buildPipelines(Engine* engine);
	void clearResources(VkDevice device);
	MaterialInstance writeMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocator& descriptorAllocator);
};

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char* name;
	VkPipeline pipeline;
	VkPipelineLayout layout;
	ComputePushConstants data;
};

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
	VkSemaphore swapchainSemaphore, renderSemaphor;
	VkFence renderFence;

	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	
	DeletionQueue deletionQueue;
	DescriptorAllocator frameDescriptors;
};

constexpr unsigned int FRAME_OVERLAP = 2;

struct Engine {
	bool resizeRequested = false;
	bool initialized = false;
	int frameNumber{ 0 };
	float renderScale = 1.f;

	VkExtent2D windowExtent{ 800, 800 };
	VkExtent2D drawExtent;

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
	AllocatedImage depthImage;

	DescriptorAllocator globalDescriptorAllocator;
	VkDescriptorSet drawImageDescriptors;
	VkDescriptorSetLayout drawImageDescriptorLayout;

	VkPipelineLayout pipelineLayout;

	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };

	VkFence immediateFence;
	VkCommandBuffer immediateCmdBuffer;
	VkCommandPool immediateCommandPool;

	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	GPUSceneData sceneData;
	VkDescriptorSetLayout gpuSceneDataDescriptorLayout;

	// default textures
	AllocatedImage whiteImage;
	AllocatedImage blackImage;
	AllocatedImage greyImage;
	AllocatedImage missingTextureImage;

	VkSampler defaultSamplerLinear;
	VkSampler defaultSamplerNearest;
	
	VkDescriptorSetLayout singleImageDescriptorLayout;

	MaterialInstance defaultData;
	GLTFMetallicRoughness metalRoughMaterial;

	DrawContext mainDrawContext;
	std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

	Camera camera;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

	EngineStats stats;

	static Engine& Get();

	void init();
	void cleanup();
	void draw();
	void run();
	void initCommands();
	FrameData& currentFrame();
	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);
	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

	AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage = VMA_MEMORY_USAGE_AUTO);
	void destroyBuffer(const AllocatedBuffer& buffer);

	AllocatedImage createImage(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroyImage(const AllocatedImage& image);

private:
	void initWindow();
	void initVulkan();
	void initSwapchain();
	void createSwapchain(int width, int height);
	void resizeSwapchain();
	void destroySwapchain();
	void initSyncStructures();
	void initAllocator();
	void initDescriptors();
	void initPipelines();
	void initBackgroundPipelines();
	void initImgui();
	void initDefaultData();
	void drawBackground(VkCommandBuffer cmdBuffer);
	void drawImGui(VkCommandBuffer cmdBuffer, VkImageView targetImageView);
	void drawGeometry(VkCommandBuffer cmdBuffer);

	AllocatedImage createImage(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);

	void updateScene();
};