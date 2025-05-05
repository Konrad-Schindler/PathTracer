#include "vk_pipelines.hpp"
#include "vk_initializers.hpp"
#include "fstream"

bool vkutil::loadShaderModule(const char* filePath, VkDevice device, VkShaderModule& outShaderModule)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		return false;
	}

	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	file.seekg(0);
	file.read((char*)buffer.data(), fileSize);
	file.close();

	VkShaderModuleCreateInfo createInfo{
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = buffer.size() * sizeof(uint32_t),
		.pCode = buffer.data()
	};

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule)) {
		return false;
	}
	outShaderModule = shaderModule;
	return true;
}

void PipelineBuilder::clear()
{
	inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

	rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };

	colorBlendAttachment = {};

	multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

	pipelineLayout = {};

	depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

	renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

	shaderStages.clear();
}

VkPipeline PipelineBuilder::build(VkDevice device)
{
	VkPipelineViewportStateCreateInfo viewportState{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	VkPipelineColorBlendStateCreateInfo colorBlending{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &colorBlendAttachment
	};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = state,
	};

	VkGraphicsPipelineCreateInfo pipelineInfo = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &renderInfo,
		.stageCount = static_cast<uint32_t>(shaderStages.size()),
		.pStages = shaderStages.data(),
		.pVertexInputState = &vertexInputInfo,
		.pInputAssemblyState = &inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &rasterizer,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depthStencil,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicInfo,
		.layout = pipelineLayout,
	};

	VkPipeline pipeline;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline)) {
		std::cout << "failed to create pipeline" << std::endl;
		return VK_NULL_HANDLE;
	}

	return pipeline;
}

void PipelineBuilder::setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
	shaderStages.clear();
	shaderStages.push_back(vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
	shaderStages.push_back(vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}

void PipelineBuilder::setInputTopology(VkPrimitiveTopology topology)
{
	inputAssembly.topology = topology;
}

void PipelineBuilder::setPolygonMode(VkPolygonMode mode)
{
	rasterizer.polygonMode = mode;
	rasterizer.lineWidth = 1.f;
}

void PipelineBuilder::setCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
	rasterizer.cullMode = cullMode;
	rasterizer.frontFace = frontFace;
}

void PipelineBuilder::setMultisamplingNone()
{
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.f;
	// other options should default to no Multisampling
}

void PipelineBuilder::setColorAttachmentFormat(VkFormat format)
{
	colorAttachmentFormat = format;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachmentFormats = &colorAttachmentFormat;
}

void PipelineBuilder::setDepthFormat(VkFormat format)
{
	renderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::enableDepthtest(bool depthWriteEnable, VkCompareOp op)
{
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = depthWriteEnable;
	depthStencil.depthCompareOp = op;
	depthStencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::disableDepthtest()
{
	depthStencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::enableBlendingAdditive()
{
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::enableBlendingAlphablend()
{
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::disableBlending()
{
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
}