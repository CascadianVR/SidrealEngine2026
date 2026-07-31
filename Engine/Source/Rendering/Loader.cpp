#define TINYGLTF3_IMPLEMENTATION
#include "Loader.h"

#include <vulkan/vulkan.h>
#include <fstream>
#include <filesystem>
#include <vk_mem_alloc.h>
#include "Logger.h"
#include "Rendering/Vulkan/VulkanCore.h"

std::unordered_map<std::string, Model> Loader::loadedModels;	

void Loader::LoadGLB(const std::string& fileName)
{
	if (loadedModels.contains(fileName))
	{
		Logger::Warn("Model already loaded: ", fileName);
		return;
	}

	Logger::Info("Loading GLB file: ", fileName);

	// Load the GLB file into memory
	std::ifstream file(fileName, std::ios::binary | std::ios::ate);
	if (!file)
	{
		Logger::Error("Failed to open file: ", fileName);
		return;
	}

	std::streamsize size = file.tellg();
	if (size < 0)
	{
		Logger::Error("Failed to get file size: ", fileName);
		return;
	}

	std::vector<uint8_t> data(static_cast<size_t>(size));
	file.seekg(0, std::ios::beg);
	if (!file.read(
		reinterpret_cast<char*>(data.data()),
		size))
	{
		Logger::Error("Failed to read file: ", fileName);
		return;
	}

	// Parse the GLB data using TinyGLTF3
	tg3_parse_options opts;
	tg3_error_stack errors;
	tg3_model model;

	tg3_parse_options_init(&opts);
	tg3_error_stack_init(&errors);

	tg3_error_code err = tg3_parse_glb(
		&model,
		&errors,
		data.data(),
		data.size(),
		std::filesystem::current_path().generic_string().c_str(),
		static_cast<uint32_t>(std::filesystem::current_path().generic_string().size()),
		&opts
	);

	if (err != 0)
	{
		Logger::Error("Failed to parse GLB file: ", fileName);
		return;
	}

	// Load vertex and index data from the model
	Model loadedModel;

	Logger::Info("Model has ", model.meshes_count, " meshes.");
	for (uint32_t i = 0; i < model.meshes_count; ++i)
	{
		Logger::Info("Processing mesh ", i, ": ", model.meshes[i].name.data);
		const tg3_mesh& mesh = model.meshes[i];
		for (uint32_t j = 0; j < mesh.primitives_count; ++j)
		{

			Logger::Info("Processing primitive ", j, " of mesh ", i);
			const tg3_primitive& primitive = mesh.primitives[j];

			// List all attributes of the primitive
			Logger::Info("Primitive ", j, " has ", primitive.attributes_count, " attributes.");
			for (uint32_t k = 0; k < primitive.attributes_count; ++k)
			{
				const tg3_str_int_pair& attribute = primitive.attributes[k];
				Logger::Info("  Attribute ", k, ": Name: ", attribute.key.data, ", Accessor Index: ", attribute.value);
			}
			
			Mesh loadedMesh;

			GetVertexData(model, primitive, loadedMesh.vertices);
			GetIndexData(model, primitive.indices, loadedMesh.indices);

			CreateVertexBuffer(loadedMesh);
			CreateIndexBuffer(loadedMesh);

			Logger::Warn("Vertex Address: ", loadedMesh.vertexBuffer);
			Logger::Warn("Index Address: ", loadedMesh.indexBuffer);

			loadedModel.meshes.push_back(loadedMesh);
		}
	}

	loadedModels[fileName] = loadedModel;
	Logger::Success("Loaded GLB file: ", fileName);
}

void Loader::GetVertexData(const tg3_model& model, const tg3_primitive& primitive, std::vector<Vertex>& vertices)
{
	int positionAccessorIndex = GetAccessorIndex(primitive, "POSITION");
	int normalAccessorIndex = GetAccessorIndex(primitive, "NORMAL");
	int uvAccessorIndex = GetAccessorIndex(primitive, "TEXCOORD_0");

	// Position data
	const tg3_accessor& positionAccessor = model.accessors[positionAccessorIndex];
	const tg3_buffer_view& positionBufferView = model.buffer_views[positionAccessor.buffer_view];
	const tg3_buffer& positionBuffer = model.buffers[positionBufferView.buffer];
	const uint8_t* positionData = positionBuffer.data.data + positionBufferView.byte_offset + positionAccessor.byte_offset;
	const uint32_t positionStride = positionBufferView.byte_stride != 0 ? positionBufferView.byte_stride : static_cast<uint32_t>(sizeof(glm::vec3));
	
	// Normal data
	const tg3_accessor& normalAccessor = model.accessors[normalAccessorIndex];
	const tg3_buffer_view& normalBufferView = model.buffer_views[normalAccessor.buffer_view];
	const tg3_buffer& normalBuffer = model.buffers[normalBufferView.buffer];
	const uint8_t* normalData = normalBuffer.data.data + normalBufferView.byte_offset + normalAccessor.byte_offset;
	const uint32_t normalStride = normalBufferView.byte_stride != 0 ? normalBufferView.byte_stride : static_cast<uint32_t>(sizeof(glm::vec3));
	
	// UV data
	const tg3_accessor& uvAccessor = model.accessors[uvAccessorIndex];
	const tg3_buffer_view& uvBufferView = model.buffer_views[uvAccessor.buffer_view];
	const tg3_buffer& uvBuffer = model.buffers[uvBufferView.buffer];
	const uint8_t* uvData = uvBuffer.data.data + uvBufferView.byte_offset + uvAccessor.byte_offset;
	const uint32_t uvStride = uvBufferView.byte_stride != 0 ? uvBufferView.byte_stride : static_cast<uint32_t>(sizeof(glm::vec2));

	// Assuming the accessor type is VEC3 and component type is FLOAT
	for (uint32_t v = 0; v < positionAccessor.count; v++)
	{
		glm::vec4 position{ 1.0f };
		std::memcpy(&position, positionStride * v + positionData, sizeof(glm::vec3));
		position.w = 1.0f;

		glm::vec4 normal{ 0.0f };
		std::memcpy(&normal, normalStride * v + normalData, sizeof(glm::vec3));

		glm::vec4 uv{ 0.0f };
		std::memcpy(&uv, uvStride * v + uvData, sizeof(glm::vec2));

		// Initialize normal and uv to sensible defaults to avoid uninitialized memory
		Vertex vert{ position, normal, uv };
		vertices.push_back(vert);
	}
}

void Loader::GetIndexData(const tg3_model& model, const uint32_t accessorIndex, std::vector<uint32_t>& indices)
{
	// Load vertex positions from the first accessor of the first primitive
	const tg3_accessor& accessor = model.accessors[accessorIndex];
	const tg3_buffer_view& bufferView = model.buffer_views[accessor.buffer_view];
	const tg3_buffer& buffer = model.buffers[bufferView.buffer];

	const uint8_t* data = buffer.data.data + bufferView.byte_offset + accessor.byte_offset;

	indices.resize(accessor.count);

	Logger::Info("Index type: ", accessor.component_type);
	switch (accessor.component_type)
	{
		case TG3_COMPONENT_TYPE_UNSIGNED_BYTE:
		{
			const uint8_t* src = reinterpret_cast<const uint8_t*>(data);

			for (uint32_t i = 0; i < accessor.count; i++)
				indices[i] = src[i];

			break;
		}

		case TG3_COMPONENT_TYPE_UNSIGNED_SHORT:
		{
			const uint16_t* src = reinterpret_cast<const uint16_t*>(data);

			for (uint32_t i = 0; i < accessor.count; i++)
				indices[i] = src[i];

			break;
		}

		case TG3_COMPONENT_TYPE_UNSIGNED_INT:
		{
			const uint32_t* src = reinterpret_cast<const uint32_t*>(data);

			for (uint32_t i = 0; i < accessor.count; i++)
				indices[i] = src[i];

			break;
		}

		default:
		{
			Logger::Error("Unsupported index component type.");
			indices.clear();
			break;
		}
	}
}

void Loader::CreateVertexBuffer(Mesh& mesh) {
	const VkDeviceSize vertexBufferSize{ sizeof(Vertex) * mesh.vertices.size() };
	VkBufferCreateInfo bufferCreateInfo {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = vertexBufferSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT; // For staging use VK_BUFFER_USAGE_TRANSFER_DST_BIT

	VmaAllocationCreateInfo bufferAllocationCreateInfo {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

	VmaAllocationInfo vertexBufferAllocationInfo{};
	if (vmaCreateBuffer(VulkanCore::GetAllocator(), &bufferCreateInfo, &bufferAllocationCreateInfo, &mesh.vertexBuffer, &mesh.vertexBufferAllocation, &vertexBufferAllocationInfo) != VK_SUCCESS)
	{
		Logger::Error("Failed to create vertex buffer for primitive");
		return;
	}
	
	// Copy vertex data into the mapped allocation
	if (vertexBufferSize > 0) {
		memcpy(vertexBufferAllocationInfo.pMappedData, mesh.vertices.data(), vertexBufferSize);
	}

	// Flush to make non-coherent memory visible to the GPU
	vmaFlushAllocation(VulkanCore::GetAllocator(), mesh.vertexBufferAllocation, 0, vertexBufferSize);

	// Get address of gpu buffer
	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = mesh.vertexBuffer;

	mesh.pushConstants.vertexBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &addressInfo);
	mesh.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
}

void Loader::CreateIndexBuffer(Mesh& mesh) {
	const VkDeviceSize indexBufferSize{ sizeof(uint32_t) * mesh.indices.size() };
	VkBufferCreateInfo bufferCreateInfo {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = indexBufferSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT; // For staging use VK_BUFFER_USAGE_TRANSFER_DST_BIT

	VmaAllocationCreateInfo bufferAllocationCreateInfo {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

	VmaAllocationInfo indexBufferAllocationInfo{};
	if (vmaCreateBuffer(VulkanCore::GetAllocator(), &bufferCreateInfo, &bufferAllocationCreateInfo, &mesh.indexBuffer, &mesh.indexBufferAllocation, &indexBufferAllocationInfo) != VK_SUCCESS)
	{
		Logger::Error("Failed to create vertex buffer for primitive");
		return;
	}
	
	// Copy index data into the mapped allocation
	if (indexBufferSize > 0) {
		memcpy(indexBufferAllocationInfo.pMappedData, mesh.indices.data(), indexBufferSize);
	}

	// Flush to make non-coherent memory visible to the GPU
	vmaFlushAllocation(VulkanCore::GetAllocator(), mesh.indexBufferAllocation, 0, indexBufferSize);

	// Get address of gpu buffer
	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = mesh.indexBuffer;

	mesh.pushConstants.indexBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &addressInfo);
	mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
}

int Loader::GetAccessorIndex(const tg3_primitive &primitive, const char* accessorName)
{
	int accessorIndex = 999;
	for (uint32_t a = 0; a < primitive.attributes_count; ++a) {
		const tg3_str_int_pair& attribute = primitive.attributes[a];
		if (std::string(attribute.key.data) == accessorName) {
			accessorIndex = attribute.value;
			break;
		}
	}

	if (accessorIndex == 999)
	{
		Logger::Error("[Loader] ", accessorName, " Accessor Index not found!");
	}

	return accessorIndex;
}
