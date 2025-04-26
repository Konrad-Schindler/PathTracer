#pragma once

#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h> 

#include <iostream>
#include <functional>

#include <vector>
#include <deque>
#include <span>


#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err != VK_SUCCESS) {                                         \
            std::cout << "Detected Vulkan error: {}" << string_VkResult(err) << std::endl; \
            abort();                                                    \
        }                                                               \
    } while (0)

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};