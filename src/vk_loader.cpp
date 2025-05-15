#include "vk_loader.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <glm/gtc/quaternion.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "engine.hpp"

std::optional<AllocatedImage> loadImage(Engine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
	AllocatedImage newImage{};
	int width, height, nrChannels;

	std::visit(
		fastgltf::visitor{
			[](auto& arg) {},
			[&](fastgltf::sources::URI& filePath) {
				assert(filePath.fileByteOffset == 0);
				assert(filePath.uri.isLocalPath());

				const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());
				unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
				if (data) {
					VkExtent3D imagesize;
					imagesize.width = width;
					imagesize.height = height;
					imagesize.depth = 1;

					newImage = engine->createImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
					stbi_image_free(data);
				}
			},
			[&](fastgltf::sources::Vector& vector) {
				unsigned char* data = stbi_load_from_memory(
					reinterpret_cast<const stbi_uc*>(vector.bytes.data()), 
					static_cast<int>(vector.bytes.size()) , 
					&width, &height, &nrChannels, 4);
				if (data) {
					VkExtent3D imagesize;
					imagesize.width = width;
					imagesize.height = height;
					imagesize.depth = 1;

					newImage = engine->createImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
					stbi_image_free(data);
				}
			},
			[&](fastgltf::sources::BufferView& view) {
				auto& bufferView = asset.bufferViews[view.bufferViewIndex];
				auto& buffer = asset.buffers[bufferView.bufferIndex];

				std::visit(fastgltf::visitor {
					[](std::monostate& arg) {
					},
					[](fastgltf::sources::BufferView& arg) {
					},
					[](fastgltf::sources::URI& arg) {
					},
					[&](fastgltf::sources::Array& arg) {
						unsigned char* data = stbi_load_from_memory(
							reinterpret_cast<const stbi_uc*>(arg.bytes.data() + bufferView.byteOffset),
							static_cast<int>(bufferView.byteLength),
							&width, &height, &nrChannels, 4);
						if (data) {
							VkExtent3D imagesize;
							imagesize.width = width;
							imagesize.height = height;
							imagesize.depth = 1;

							newImage = engine->createImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
							stbi_image_free(data);
						}
					},
					[](fastgltf::sources::CustomBuffer& arg) {
					},
					[](fastgltf::sources::ByteView& arg) {
					},
					[](fastgltf::sources::Fallback& arg) {
					},
					[&](fastgltf::sources::Vector& vector) {
						unsigned char* data = stbi_load_from_memory(
							reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset),
							static_cast<int>(bufferView.byteLength),
							&width, &height, &nrChannels, 4);
						if (data) {
							VkExtent3D imagesize;
							imagesize.width = width;
							imagesize.height = height;
							imagesize.depth = 1;

							newImage = engine->createImage(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
							stbi_image_free(data);
						}
					} 
				}, buffer.data);
			},
		}, image.data);

	if (newImage.image == VK_NULL_HANDLE) {
		return {};
	}
	else {
		return newImage;
	}
}

static VkFilter extractFilter(fastgltf::Filter filter)
{
	switch (filter) {
	case fastgltf::Filter::Nearest:
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::NearestMipMapLinear:
		return VK_FILTER_NEAREST;

	case fastgltf::Filter::Linear:
	case fastgltf::Filter::LinearMipMapNearest:
	case fastgltf::Filter::LinearMipMapLinear:
	default:
		return VK_FILTER_LINEAR;
	}
}

static VkSamplerMipmapMode extractMipmapMode(fastgltf::Filter filter)
{
	switch (filter) {
	case fastgltf::Filter::NearestMipMapNearest:
	case fastgltf::Filter::LinearMipMapNearest:
		return VK_SAMPLER_MIPMAP_MODE_NEAREST;

	case fastgltf::Filter::NearestMipMapLinear:
	case fastgltf::Filter::LinearMipMapLinear:
	default:
		return VK_SAMPLER_MIPMAP_MODE_LINEAR;
	}
}

std::optional<std::shared_ptr<LoadedGLTF>> loadGLTF(Engine* engine, std::filesystem::path filePath)
{
	std::cout << "Loading glTF: " << filePath << std::endl;

	std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
	scene->creator = engine;
	LoadedGLTF& file = *scene.get();

	fastgltf::Parser parser{};
	fastgltf::Asset gltf;

	constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember 
		| fastgltf::Options::AllowDouble 
		| fastgltf::Options::LoadGLBBuffers 
		| fastgltf::Options::LoadExternalBuffers;

	auto data = fastgltf::GltfDataBuffer::FromPath(filePath);
	if (!data) {
		std::cout << "Failed to read glTF: " << fastgltf::to_underlying(data.error()) << std::endl;
		return {};
	}

	fastgltf::GltfType type = fastgltf::determineGltfFileType(data.get());

	if (type == fastgltf::GltfType::glTF) {
		auto load = parser.loadGltf(data.get(), filePath.parent_path(), gltfOptions);
		if (load) {
			gltf = std::move(load.get());
		}
		else {
			std::cout << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
			return {};
		}
	}
	else if (type == fastgltf::GltfType::GLB) {
		auto load = parser.loadGltfBinary(data.get(), filePath.parent_path(), gltfOptions);
		if (load) {
			gltf = std::move(load.get());
		}
		else {
			std::cout << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << std::endl;
			return {};
		}
	}
	else {
		std::cout << "Failed loading GLTF: Unsupported type!" << std::endl;
	}

	std::vector<DescriptorAllocator::PoolSizeRatio> sizes = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
	};

	if (gltf.materials.size() > 0) {
		file.descriptorPool.init(engine->device, gltf.materials.size(), sizes);
	}

	for (fastgltf::Sampler& sampler : gltf.samplers) {
		VkSamplerCreateInfo samplerInfo{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = extractFilter(sampler.magFilter.value_or(fastgltf::Filter::Nearest)),
			.minFilter = extractFilter(sampler.minFilter.value_or(fastgltf::Filter::Nearest)),
			.mipmapMode = extractMipmapMode(sampler.minFilter.value_or(fastgltf::Filter::Nearest)),
			.maxLod = VK_LOD_CLAMP_NONE,
		};

		VkSampler newSampler;
		vkCreateSampler(engine->device, &samplerInfo, nullptr, &newSampler);
		file.samplers.push_back(newSampler);
	}

	std::vector<std::shared_ptr<MeshAsset>> meshes;
	std::vector<std::shared_ptr<Node>> nodes;
	std::vector<AllocatedImage> images;
	std::vector<std::shared_ptr<GLTFMaterial>> materials;

	for (fastgltf::Image& image : gltf.images) {
		auto allocImage = loadImage(engine, gltf, image);

		if (allocImage.has_value()) {
			images.push_back(*allocImage);
			file.images[image.name.c_str()] = *allocImage;
		}
		else {
			images.push_back(engine->missingTextureImage);
			std::cout << "failed to load texture " << image.name << " from gltf" << std::endl;
		}
	}

	if (gltf.materials.size() > 0) {
		file.materialDataBuffer = engine->createBuffer(
			sizeof(GLTFMetallicRoughness::MaterialConstants) * gltf.materials.size(),
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	}
	else {
		materials.push_back(std::make_shared<GLTFMaterial>());
	}
	int dataIndex = 0;
	auto sceneMaterialConstants = (GLTFMetallicRoughness::MaterialConstants*)file.materialDataBuffer.info.pMappedData;

	for (fastgltf::Material& material : gltf.materials) {
		std::shared_ptr<GLTFMaterial> newMaterial = std::make_shared<GLTFMaterial>();
		materials.push_back(newMaterial);
		file.materials[material.name.c_str()] = newMaterial;

		GLTFMetallicRoughness::MaterialConstants constants{
			.colorFactors = {
				material.pbrData.baseColorFactor[0],
				material.pbrData.baseColorFactor[1],
				material.pbrData.baseColorFactor[2],
				material.pbrData.baseColorFactor[3]
			},
			.metalRoughFactors{
				material.pbrData.metallicFactor,
				material.pbrData.roughnessFactor,
				0, 0
			}
		};
		sceneMaterialConstants[dataIndex] = constants;

		MaterialPass passType = MaterialPass::Opaque;
		if (material.alphaMode == fastgltf::AlphaMode::Blend) {
			passType = MaterialPass::Transparent;
		}

		GLTFMetallicRoughness::MaterialResources materialResources;

		materialResources.dataBuffer = file.materialDataBuffer.buffer;
		materialResources.dataBufferOffset = dataIndex * sizeof(GLTFMetallicRoughness::MaterialConstants);

		materialResources.colorImage = engine->whiteImage;
		materialResources.colorSampler = engine->defaultSamplerLinear;
		materialResources.metalRoughImage = engine->whiteImage;
		materialResources.metalRoughSampler = engine->defaultSamplerLinear;

		if (material.pbrData.baseColorTexture.has_value()) {
			fastgltf::Texture texture = gltf.textures[material.pbrData.baseColorTexture.value().textureIndex];
			size_t img = texture.imageIndex.value();
			size_t sampler = texture.samplerIndex.value();

			materialResources.colorImage = images[img];
			materialResources.colorSampler = file.samplers[sampler];
		}

		newMaterial->data = engine->metalRoughMaterial.writeMaterial(engine->device, passType, materialResources, file.descriptorPool);

		dataIndex++;
	}

	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;
	for (fastgltf::Mesh& mesh : gltf.meshes) {
		std::shared_ptr<MeshAsset> newMesh = std::make_shared<MeshAsset>();
		meshes.push_back(newMesh);
		file.meshes[mesh.name.c_str()] = newMesh;
		newMesh->name = mesh.name;

		vertices.clear();
		indices.clear();

		for (auto&& primitive : mesh.primitives) {
			GeoSurface newSurface{
				.startIndex = static_cast<uint32_t>(indices.size()),
				.count = static_cast<uint32_t>(gltf.accessors[primitive.indicesAccessor.value()].count)
			};

			size_t initialVtx = vertices.size();

			{
				fastgltf::Accessor& indexaccessor = gltf.accessors[primitive.indicesAccessor.value()];
				indices.reserve(indices.size() + indexaccessor.count);

				fastgltf::iterateAccessor<uint32_t>(gltf, indexaccessor,
					[&](uint32_t index) {
						indices.push_back(initialVtx + index);
					});
			}

			{
				fastgltf::Accessor& posAccessor = gltf.accessors[primitive.findAttribute("POSITION")->accessorIndex];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor,
					[&](glm::vec3 pos, size_t index) {
						Vertex newVtx{
							.position = pos,
							.normal = { 1, 0, 0 },
							.color = glm::vec4{ 1.f },
						};
						vertices[initialVtx + index] = newVtx;
					});
			}

			auto normals = primitive.findAttribute("NORMAL");
			if (normals != primitive.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex],
					[&](glm::vec3 normal, size_t index) {
						vertices[initialVtx + index].normal = normal;
					});
			}

			auto uv = primitive.findAttribute("TEXCOORD_0");
			if (uv != primitive.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->accessorIndex],
					[&](glm::vec2 uv, size_t index) {
						vertices[initialVtx + index].uv_x = uv.x;
						vertices[initialVtx + index].uv_y = uv.y;
					});
			}

			auto colors = primitive.findAttribute("COLOR_0");
			if (colors != primitive.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->accessorIndex],
					[&](glm::vec4 color, size_t index) {
						vertices[initialVtx + index].color = color;
					});
			}

			if (primitive.materialIndex.has_value()) {
				newSurface.material = materials[primitive.materialIndex.value()];
			}
			else {
				newSurface.material = materials[0];
			}

			newMesh->surfaces.push_back(newSurface);
		}

		newMesh->meshBuffers = engine->uploadMesh(indices, vertices);
	}

	for (fastgltf::Node& node : gltf.nodes) {
		std::shared_ptr<Node> newNode;

		if (node.meshIndex.has_value()) {
			newNode = std::make_shared<MeshNode>();
			static_cast<MeshNode*>(newNode.get())->mesh = meshes[node.meshIndex.value()];
		}
		else {
			newNode = std::make_shared<Node>();
		}

		nodes.push_back(newNode);
		file.nodes[node.name.c_str()] = newNode;

		std::visit(fastgltf::visitor{
			[&](fastgltf::math::fmat4x4 matrix) {
				memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
			},
			[&](fastgltf::TRS transform) {
				glm::vec3 translation(transform.translation[0], transform.translation[1],transform.translation[2]);
				glm::quat rotation(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
				glm::vec3 scaling(transform.scale[0], transform.scale[1], transform.scale[2]);

				glm::mat4 translationMatrix = glm::translate(glm::mat4(1.f), translation);
				glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
				glm::mat4 scalingMatrix = glm::scale(glm::mat4(1.f), scaling);

				newNode->localTransform = translationMatrix * rotationMatrix * scalingMatrix;
			}
		}, node.transform);
	}

	for (int i = 0; i < gltf.nodes.size(); i++) {
		fastgltf::Node& node = gltf.nodes[i];
		std::shared_ptr<Node>& sceneNode = nodes[i];

		for (size_t child : node.children) {
			sceneNode->children.push_back(nodes[child]);
			nodes[child]->parent = sceneNode;
		}
	}

	for (auto& node : nodes) {
		if (node->parent.lock() == nullptr) {
			file.topNodes.push_back(node);
			node->refreshTransform(glm::mat4{ 1.f });
		}
	}

	std::cout << "Finished loading GLTF" << std::endl;

	return scene;
}

void LoadedGLTF::draw(const glm::mat4& topMatrix, DrawContext& context)
{
	for (auto& node : topNodes) {
		node->draw(topMatrix, context);
	}
}

void LoadedGLTF::clearAll()
{
	VkDevice device = creator->device;
	descriptorPool.destroyPools(device);
	creator->destroyBuffer(materialDataBuffer);

	for (auto& [_, v] : meshes) {
		creator->destroyBuffer(v->meshBuffers.indexBuffer);
		creator->destroyBuffer(v->meshBuffers.vertexBuffer);
	}

	for (auto& [_, v] : images) {
		if (v.image == creator->missingTextureImage.image) continue;
		creator->destroyImage(v);
	}

	for (auto& sampler : samplers) {
		vkDestroySampler(device, sampler, nullptr);
	}
}
