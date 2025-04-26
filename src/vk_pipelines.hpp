#include "vk_types.hpp"

namespace vkutil {
	bool loadShaderModule(const char* filePath, VkDevice device, VkShaderModule& outShaderModule);
}