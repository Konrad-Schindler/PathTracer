#include "vk_pipelines.hpp"
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
