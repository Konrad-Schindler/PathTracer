#pragma once
#include "vk_types.hpp"

struct PipelineBuilder {
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineLayout pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo depthStencil;
	VkPipelineRenderingCreateInfo renderInfo;
	VkFormat colorAttachmentFormat;

	PipelineBuilder() {
		clear();
	}

	void clear();
	VkPipeline build(VkDevice device);
	void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
	void setInputTopology(VkPrimitiveTopology topology);
	void setPolygonMode(VkPolygonMode mode);
	void setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
	void setMultisamplingNone();
	void setColorAttachmentFormat(VkFormat format);
	void setDepthFormat(VkFormat format);
	void enableDepthtest(bool depthWriteEnable, VkCompareOp op);
	void disableDepthtest();
	void enableBlendingAdditive();
	void enableBlendingAlphablend();
	void disableBlending();
};


namespace vkutil {
	bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule& outShaderModule);
}