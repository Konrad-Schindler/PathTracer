#include "vk_loader.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>

#include "engine.hpp"

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(Engine& engine, std::filesystem::path filePath)
{
	std::cout << "Loading glTF: " << filePath << std::endl;

	auto gltfFile = fastgltf::GltfDataBuffer::FromPath(filePath);
	if (!gltfFile) {
		std::cout << "Failed to read glTF: " << fastgltf::to_underlying(gltfFile.error()) << std::endl;
		return {};
	}
	constexpr auto gltfOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

	fastgltf::Asset gltf;
	fastgltf::Parser parser{};

	auto load = parser.loadGltfBinary(gltfFile.get(), filePath.parent_path(), gltfOptions);

	if (!load) {
		std::cout << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) <<  std::endl;
		return {};
	}
	else {
		gltf = std::move(load.get());
	}

	std::vector<std::shared_ptr<MeshAsset>> meshes;
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;
	for (fastgltf::Mesh& mesh : gltf.meshes) {
		vertices.clear();
		indices.clear();

		MeshAsset newMesh;
		newMesh.name = mesh.name;

		for (auto&& p : mesh.primitives) {
			GeoSurface newSurface{
				.startIndex = static_cast<uint32_t>(indices.size()),
				.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count)
			};

			size_t initialVtx = vertices.size();

			{
				fastgltf::Accessor& indexaccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexaccessor.count);

				fastgltf::iterateAccessor<uint32_t>(gltf, indexaccessor,
					[&](uint32_t index) {
						indices.push_back(initialVtx + index);
					});
			}

			{
				fastgltf::Accessor& posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
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

			auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[normals->accessorIndex],
					[&](glm::vec3 normal, size_t index) {
						vertices[initialVtx + index].normal = normal;
					});
			}

			auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[uv->accessorIndex],
					[&](glm::vec2 uv, size_t index) {
						vertices[initialVtx + index].uv_x = uv.x;
						vertices[initialVtx + index].uv_y = uv.y;
					});
			}

			auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[colors->accessorIndex],
					[&](glm::vec4 color, size_t index) {
						vertices[initialVtx + index].color = color;
					});
			}

			newMesh.surfaces.push_back(newSurface);
		}

		constexpr bool OverrideColors = true;
		if (OverrideColors) {
			for (Vertex& vtx : vertices) {
				vtx.color = glm::vec4(vtx.normal, 1.f);
			}
		}

		newMesh.meshBuffers = engine.uploadMesh(indices, vertices);
		meshes.push_back(std::make_shared<MeshAsset>(std::move(newMesh)));
	}

	return meshes;
}
