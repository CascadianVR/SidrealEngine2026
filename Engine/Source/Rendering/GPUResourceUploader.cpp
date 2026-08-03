#include "GPUResourceUploader.h"

#include "Rendering/Vulkan/VulkanCore.h"
#include "Logger.h"

void GPUResourceUploader::CreateDataBuffers(const std::vector<Model>& models)
{
	CreateVertexBuffer(models);
	CreateIndexBuffer(models);
	CreateInstanceDataBuffer(models);
	CreateRenderDataBuffer(models);
	CreateDescriptorSet();
}

void GPUResourceUploader::CreateVertexBuffer(const std::vector<Model>& models) {
	size_t totalVertexCount = 0;
	for (const auto& model : models)
	{
		for (const auto& mesh : model.meshes)
		{
			totalVertexCount += mesh.vertices.size();
		}
	}

	std::vector<Vertex> vertices;
	vertices.reserve(totalVertexCount);

	// Copy vertices and compute offsets per mesh
	size_t vertexOffset = 0;
	for (const auto& model : models)
	{
		for (const auto& mesh : model.meshes)
		{
			vertices.insert(vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
			vertexOffset += mesh.vertexCount;
		}
	}

	const VkDeviceSize vertexBufferSize{ sizeof(Vertex) * vertices.size() };
	VkBufferCreateInfo bufferCreateInfo {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = vertexBufferSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	VmaAllocationCreateInfo bufferAllocationCreateInfo {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

	VmaAllocationInfo vertexBufferAllocationInfo{};
	if (vmaCreateBuffer(VulkanCore::GetAllocator(), &bufferCreateInfo, &bufferAllocationCreateInfo, &m_vertexBuffer, &m_vertexBufferAllocation, &vertexBufferAllocationInfo) != VK_SUCCESS)
	{
		Logger::Error("Failed to create vertex buffer");
		return;
	}

	if (vertexBufferSize > 0) {
		memcpy(vertexBufferAllocationInfo.pMappedData, vertices.data(), vertexBufferSize);
	}

	vmaFlushAllocation(VulkanCore::GetAllocator(), m_vertexBufferAllocation, 0, vertexBufferSize);

	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = m_vertexBuffer;

	m_pushConstants.vertexBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &addressInfo);

	Logger::Success("Created vertex buffer");
}

void GPUResourceUploader::CreateIndexBuffer(const std::vector<Model>& models) {
	size_t totalIndexCount = 0;
	for (const auto& model : models)
	{
		for (const auto& mesh : model.meshes)
		{
			totalIndexCount += mesh.indices.size();
		}
	}

	std::vector<uint32_t> indices;
	indices.reserve(totalIndexCount);

	for (const auto& model : models)
	{
		for (const auto& mesh : model.meshes)
		{
			indices.insert(indices.end(), mesh.indices.begin(), mesh.indices.end());
		}
	}

	const VkDeviceSize indexBufferSize{ sizeof(uint32_t) * indices.size() };
	VkBufferCreateInfo bufferCreateInfo {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = indexBufferSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	VmaAllocationCreateInfo bufferAllocationCreateInfo {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

	VmaAllocationInfo indexBufferAllocationInfo{};
	if (vmaCreateBuffer(VulkanCore::GetAllocator(), &bufferCreateInfo, &bufferAllocationCreateInfo, &m_indexBuffer, &m_indexBufferAllocation, &indexBufferAllocationInfo) != VK_SUCCESS)
	{
		Logger::Error("Failed to create index buffer");
		return;
	}

	if (indexBufferSize > 0) {
		memcpy(indexBufferAllocationInfo.pMappedData, indices.data(), indexBufferSize);
	}

	vmaFlushAllocation(VulkanCore::GetAllocator(), m_indexBufferAllocation, 0, indexBufferSize);

	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = m_indexBuffer;

	m_pushConstants.indexBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &addressInfo);

	Logger::Success("Created index buffer");
}

void GPUResourceUploader::CreateRenderDataBuffer(const std::vector<Model>& models)
{
	m_renderData.clear();
	uint32_t instanceOffset = 0;
	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	for (const auto& model : models)
	{
		for (const auto& mesh : model.meshes)
		{
			m_renderData.emplace_back(RenderData{
				.vertexOffset = vertexOffset,
				.indexOffset = indexOffset,
				.indexCount = mesh.indexCount,
				.instanceOffset = instanceOffset,
				.instanceCount = model.instanceCount,
			});
			vertexOffset += mesh.vertexCount;
			indexOffset += mesh.indexCount;
			instanceOffset += model.instanceCount;
		}
	}

	const VkDeviceSize drawCommandBufferSize{ sizeof(RenderData) * m_renderData.size() };
	VkBufferCreateInfo bufferCreateInfo {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = drawCommandBufferSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	VmaAllocationCreateInfo bufferAllocationCreateInfo {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

	VmaAllocationInfo bufferAllocationInfo{};
	if (vmaCreateBuffer(VulkanCore::GetAllocator(), &bufferCreateInfo, &bufferAllocationCreateInfo, &m_renderDataBuffer, &m_renderDataBufferAllocation, &bufferAllocationInfo) != VK_SUCCESS)
	{
		Logger::Error("Failed to create render data buffer");
		return;
	}

	if (drawCommandBufferSize > 0) {
		memcpy(bufferAllocationInfo.pMappedData, m_renderData.data(), drawCommandBufferSize);
	}

	vmaFlushAllocation(VulkanCore::GetAllocator(), m_renderDataBufferAllocation, 0, drawCommandBufferSize);

	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = m_renderDataBuffer;

	m_pushConstants.renderDataBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &addressInfo);

	Logger::Success("Created render data buffer");
}

void GPUResourceUploader::CreateInstanceDataBuffer(const std::vector<Model>& models) {
	m_instanceData.clear();
	for (const auto& model : models)
	{
		for (size_t i = 0; i < model.meshes.size(); i++)
		{
			for (uint32_t j = 0; j < model.instanceCount; j++)
			{
				m_instanceData.emplace_back(InstanceData{ .modelMatrix = model.instanceMatrices[j] });
			}
		}
	}

	const VkDeviceSize bufferSize{ sizeof(InstanceData) * m_instanceData.size() };
	VkBufferCreateInfo bufferCreateInfo {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = bufferSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

	VmaAllocationCreateInfo bufferAllocationCreateInfo {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

	VmaAllocationInfo bufferAllocationInfo{};
	if (vmaCreateBuffer(VulkanCore::GetAllocator(), &bufferCreateInfo, &bufferAllocationCreateInfo, &dataBuffer, &m_instanceDataBufferAllocation, &bufferAllocationInfo) != VK_SUCCESS)
	{
		Logger::Error("Failed to create instance data buffer");
		return;
	}

	if (bufferSize > 0) {
		memcpy(bufferAllocationInfo.pMappedData, m_instanceData.data(), bufferSize);
	}

	vmaFlushAllocation(VulkanCore::GetAllocator(), m_instanceDataBufferAllocation, 0, bufferSize);

	VkBufferDeviceAddressInfo addressInfo{};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = dataBuffer;

	m_pushConstants.instanceDataBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &addressInfo);

	Logger::Success("Created instance data buffer");
}

void GPUResourceUploader::CreateDescriptorSet()
{
	m_bindings.push_back({
		.binding = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
	});

	m_bindings.push_back({
		.binding = 1,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
	});

	m_bindings.push_back({
		.binding = 2,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
	});

	m_bindings.push_back({
		.binding = 3,
		.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
	});

	m_bindings.push_back({
		.binding = 4,
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount = MAX_TEXTURES,
		.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
	});

	m_bindingFlags[4] =
	  VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
	| VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
	| VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
	flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	flagsInfo.bindingCount = static_cast<uint32_t>(m_bindingFlags.size());
	flagsInfo.pBindingFlags = m_bindingFlags.data();

	VkDescriptorSetLayoutCreateInfo layoutInfo;
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = &flagsInfo;
	layoutInfo.bindingCount = static_cast<uint32_t>(m_bindings.size());
	layoutInfo.pBindings = m_bindings.data();
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

	if (vkCreateDescriptorSetLayout(VulkanCore::GetDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
	{
		Logger::Error("Failed to create descriptor set layout");
		return;
	}

	constexpr VkDescriptorPoolSize poolSizes[] =
	{
		{
			.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 4
		},
		{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = MAX_TEXTURES
		}
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = std::size(poolSizes);
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = 1;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

	if (vkCreateDescriptorPool(VulkanCore::GetDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
	{
		Logger::Error("Failed to create descriptor pool");
		return;
	}

	constexpr uint32_t textureCount = MAX_TEXTURES;
	VkDescriptorSetVariableDescriptorCountAllocateInfo variableInfo{};
	variableInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	variableInfo.descriptorSetCount = 1;
	variableInfo.pDescriptorCounts = &textureCount;

	VkDescriptorSetAllocateInfo allocInfo;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_descriptorSetLayout;
	allocInfo.pNext = &variableInfo;

	if (vkAllocateDescriptorSets(VulkanCore::GetDevice(), &allocInfo, &m_descriptorSet) != VK_SUCCESS)
	{
		Logger::Error("Failed to allocate descriptor set");
		return;
	}

	VkDescriptorBufferInfo bufferInfos[4]{};
	bufferInfos[0].buffer = m_vertexBuffer;
	bufferInfos[0].range = VK_WHOLE_SIZE;
	bufferInfos[1].buffer = m_indexBuffer;
	bufferInfos[1].range = VK_WHOLE_SIZE;
	bufferInfos[2].buffer = dataBuffer;
	bufferInfos[2].range = VK_WHOLE_SIZE;
	bufferInfos[3].buffer = m_renderDataBuffer;
	bufferInfos[3].range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet writes[4]{};
	for (int i = 0; i < 4; i++)
	{
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = m_descriptorSet;
		writes[i].dstBinding = i;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[i].pBufferInfo = &bufferInfos[i];
	}

	vkUpdateDescriptorSets(VulkanCore::GetDevice(), 4, writes, 0, nullptr);

	Logger::Success("Created descriptor set");
}
