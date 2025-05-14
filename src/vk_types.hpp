#pragma once

#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h> 

#include <iostream>
#include <functional>

#include <vector>
#include <deque>
#include <span>
#include <array>

#include <glm/glm.hpp>

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
    VkImageLayout currentLayout;
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct GPUMeshBuffers {
    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

struct GPUDrawPushConstants {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

enum MaterialPass {
    Opaque,
    Transparent,
};

struct MaterialPipeline {
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance {
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};

struct RenderObject {
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance* material;
    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
};

struct GPUSceneData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewprojection;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDrection;
    glm::vec4 sunlightColor;
};

struct DrawContext {
    std::vector<RenderObject> opaqueSurfaces;
    std::vector<RenderObject> transparentSurfaces;
};

struct Renderable {
    virtual void draw(const glm::mat4& topMatrix, DrawContext& context) = 0;
};

struct Node : Renderable {
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform = glm::mat4{ 1.f };
    glm::mat4 globalTransform = glm::mat4{ 1.f };

    void refreshTransform(const glm::mat4& parentMatrix);
    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};