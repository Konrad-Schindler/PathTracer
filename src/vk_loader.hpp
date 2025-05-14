#pragma once
#include <filesystem>
#include <unordered_map>

#include "vk_descriptors.hpp"
#include "vk_types.hpp"

struct Engine;

struct Bounds {
	glm::vec3 origin;
	float sphereRadius;
	glm::vec3 extents;
};

struct GLTFMaterial {
	MaterialInstance data;
};

struct GeoSurface {
	uint32_t startIndex;
	uint32_t count;
	std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset {
	std::string name;
	std::vector<GeoSurface> surfaces;
	GPUMeshBuffers meshBuffers;
};

struct LoadedGLTF : Renderable {
	std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
	std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
	std::unordered_map<std::string, AllocatedImage> images;
	std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

	std::vector<std::shared_ptr<Node>> topNodes;

	std::vector<VkSampler> samplers;

	DescriptorAllocator descriptorPool;

	AllocatedBuffer materialDataBuffer;

	Engine* creator;

	~LoadedGLTF() { clearAll(); };

	virtual void draw(const glm::mat4& topMatrix, DrawContext& context) override;

private:
	void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGLTF(Engine* engine, std::filesystem::path filePath);