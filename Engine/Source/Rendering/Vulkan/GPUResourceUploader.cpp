#include "GPUResourceUploader.h"

#include <queue>

#include "Rendering/Vulkan/VulkanCore.h"
#include "Logger.h"
#include "Rendering/Loader.h"

void GPUResourceUploader::CreateDataBuffers()
{
	std::vector<Model>& models = Loader::GetLoadedModels();
	const std::vector<Texture>& textures = Loader::GetLoadedTextures();
	
	CreateVertexBuffer(models);
	CreateIndexBuffer(models);
	CreateInstanceDataBuffer(models);
	CreateRenderDataBuffer(models);
	CreateTextureDataBuffer(textures);
	CreateSampler();
	CreateDescriptorSet();
}

void GPUResourceUploader::CreateVertexBuffer(std::vector<Model>& models) {
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
	for (auto& model : models)
	{
		for (auto& mesh : model.meshes)
		{
			vertices.insert(vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
			mesh.vertexOffset = static_cast<uint32_t>(vertexOffset);
			vertexOffset += mesh.vertexCount;
		}
	}

	const VkDeviceSize vertexBufferSize{ sizeof(Vertex) * vertices.size() };
	VkBufferCreateInfo bufferCreateInfo {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = vertexBufferSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

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

	m_vertexBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &addressInfo);
	
	Logger::Success("Created vertex buffer");
}

void GPUResourceUploader::CreateIndexBuffer(std::vector<Model>& models) {
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

	uint32_t indexOffset = 0;
	for (auto& model : models)
	{
		for (auto& mesh : model.meshes)
		{
			indices.insert(indices.end(), mesh.indices.begin(), mesh.indices.end());
			mesh.indexOffset = indexOffset;
			indexOffset += mesh.indexCount;
		}
	}

	const VkDeviceSize indexBufferSize{ sizeof(uint32_t) * indices.size() };
	VkBufferCreateInfo bufferCreateInfo {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = indexBufferSize;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 
		| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
		| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

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

	m_indexBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &addressInfo);
	
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
				.textureIndex = mesh.textureIndex
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

	m_renderDataBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &addressInfo);

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
	if (vmaCreateBuffer(VulkanCore::GetAllocator(), &bufferCreateInfo, &bufferAllocationCreateInfo, &m_instanceDataBuffer, &m_instanceDataBufferAllocation, &bufferAllocationInfo) != VK_SUCCESS)
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
	addressInfo.buffer = m_instanceDataBuffer;

	m_instanceDataBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &addressInfo);

	Logger::Success("Created instance data buffer");
}

void GPUResourceUploader::CreateTextureDataBuffer(const std::vector<Texture>& textures)
{
	VkCommandPoolCreateInfo poolCI{};
	poolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCI.queueFamilyIndex = VulkanCore::GetQueueFamilyIndex();
	poolCI.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	VkCommandPool uploadPool;
	if (vkCreateCommandPool(VulkanCore::GetDevice(), &poolCI, nullptr, &uploadPool) != VK_SUCCESS)
	{
		Logger::Error("Failed to create upload command pool");
		return;
	}

	VkFenceCreateInfo fenceCI{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
	VkFence fenceOneTime{};
	if (vkCreateFence(VulkanCore::GetDevice(), &fenceCI, nullptr, &fenceOneTime) != VK_SUCCESS)
	{
		Logger::Error("Failed to create upload fence");
		vkDestroyCommandPool(VulkanCore::GetDevice(), uploadPool, nullptr);
		return;
	}

	m_textureImages.resize(textures.size());
	m_textureImageViews.resize(textures.size());
	m_textureAllocations.resize(textures.size());

	for (size_t i = 0; i < textures.size(); i++)
	{
		const Texture& texture = textures[i];

		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageCreateInfo.extent = { static_cast<uint32_t>(texture.width), static_cast<uint32_t>(texture.height), 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VmaAllocationCreateInfo imageAllocCI{};
		imageAllocCI.usage = VMA_MEMORY_USAGE_AUTO;

		if (vmaCreateImage(VulkanCore::GetAllocator(), &imageCreateInfo, &imageAllocCI, &m_textureImages[i], &m_textureAllocations[i], nullptr) != VK_SUCCESS)
		{
			Logger::Error("Failed to create texture image");
			return;
		}

		VkImageViewCreateInfo ivCI{};
		ivCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		ivCI.image = m_textureImages[i];
		ivCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		ivCI.format = VK_FORMAT_R8G8B8A8_SRGB;
		ivCI.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		if (vkCreateImageView(VulkanCore::GetDevice(), &ivCI, nullptr, &m_textureImageViews[i]) != VK_SUCCESS)
		{
			Logger::Error("Failed to create texture image view");
			return;
		}

		VkBuffer stagingBuffer{};
		VmaAllocation stagingAllocation{};
		VkBufferCreateInfo stagingBI{};
		stagingBI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		stagingBI.size = texture.pixels.size();
		stagingBI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VmaAllocationCreateInfo stagingAllocCI{};
		stagingAllocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		stagingAllocCI.usage = VMA_MEMORY_USAGE_AUTO;

		VmaAllocationInfo stagingAllocInfo{};
		if (vmaCreateBuffer(VulkanCore::GetAllocator(), &stagingBI, &stagingAllocCI, &stagingBuffer, &stagingAllocation, &stagingAllocInfo) != VK_SUCCESS)
		{
			Logger::Error("Failed to create staging buffer");
			return;
		}

		memcpy(stagingAllocInfo.pMappedData, texture.pixels.data(), texture.pixels.size());
		vmaFlushAllocation(VulkanCore::GetAllocator(), stagingAllocation, 0, texture.pixels.size());

		VkCommandBuffer cb{};
		VkCommandBufferAllocateInfo allocCI{};
		allocCI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocCI.commandPool = uploadPool;
		allocCI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocCI.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(VulkanCore::GetDevice(), &allocCI, &cb) != VK_SUCCESS)
		{
			Logger::Error("Failed to allocate command buffer");
			vmaDestroyBuffer(VulkanCore::GetAllocator(), stagingBuffer, stagingAllocation);
			return;
		}

		VkCommandBufferBeginInfo beginBI{};
		beginBI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginBI.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(cb, &beginBI);

		VkImageMemoryBarrier2 toTransfer{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.image = m_textureImages[i],
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};
		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &toTransfer;
		vkCmdPipelineBarrier2(cb, &depInfo);

		VkBufferImageCopy region{};
		region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.imageExtent = { static_cast<uint32_t>(texture.width), static_cast<uint32_t>(texture.height), 1 };
		vkCmdCopyBufferToImage(cb, stagingBuffer, m_textureImages[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		VkImageMemoryBarrier2 toRead{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
			.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.image = m_textureImages[i],
			.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
		};
		depInfo.pImageMemoryBarriers = &toRead;
		vkCmdPipelineBarrier2(cb, &depInfo);

		vkEndCommandBuffer(cb);

		vkResetFences(VulkanCore::GetDevice(), 1, &fenceOneTime);
		VkCommandBufferSubmitInfo cbSI{};
		cbSI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		cbSI.commandBuffer = cb;
		VkSubmitInfo2 submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
		submitInfo.commandBufferInfoCount = 1;
		submitInfo.pCommandBufferInfos = &cbSI;
		if (vkQueueSubmit2(VulkanCore::GetQueue(), 1, &submitInfo, fenceOneTime) != VK_SUCCESS)
		{
			Logger::Error("Failed to submit upload command buffer");
		}
		vkWaitForFences(VulkanCore::GetDevice(), 1, &fenceOneTime, VK_TRUE, UINT64_MAX);

		vkFreeCommandBuffers(VulkanCore::GetDevice(), uploadPool, 1, &cb);
		vmaDestroyBuffer(VulkanCore::GetAllocator(), stagingBuffer, stagingAllocation);

		Logger::Success("Uploaded texture ", i, ": ", texture.width, "x", texture.height);
	}

	vkDestroyCommandPool(VulkanCore::GetDevice(), uploadPool, nullptr);
	vkDestroyFence(VulkanCore::GetDevice(), fenceOneTime, nullptr);
}

void GPUResourceUploader::CreateSampler()
{
	VkSamplerCreateInfo samplerCI{};
	samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCI.magFilter = VK_FILTER_LINEAR;
	samplerCI.minFilter = VK_FILTER_LINEAR;
	samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCI.anisotropyEnable = VK_TRUE;
	samplerCI.maxAnisotropy = 16.0f; // Should query VkPhysicalDeviceLimits::maxSamplerAnisotropy and clamp
	samplerCI.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCI.unnormalizedCoordinates = VK_FALSE;
	samplerCI.compareEnable = VK_FALSE;
	samplerCI.compareOp = VK_COMPARE_OP_ALWAYS;

	if (vkCreateSampler(VulkanCore::GetDevice(), &samplerCI, nullptr, &m_sampler) != VK_SUCCESS)
	{
		Logger::Error("Failed to create sampler");
		return;
	}

	Logger::Success("Created sampler");
}

void GPUResourceUploader::CreateDescriptorSet()
{
	m_bindings.clear();
	
	// Vertices
	m_bindings.push_back({ .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT });
	
	// Indices
	m_bindings.push_back({ .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT });
 
	// Render Data
	m_bindings.push_back({ .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT });

	// Instance Data
	m_bindings.push_back({ .binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_VERTEX_BIT });

	// Sampler Data
	m_bindings.push_back({ .binding = 4, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT });

	// Texture Data
	m_bindings.push_back({.binding = 5, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = MAX_TEXTURES, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT });

	// WARNING FRAGILE
	m_bindingFlags[5] =
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
			.type = VK_DESCRIPTOR_TYPE_SAMPLER,
			.descriptorCount = 1
		},
		{
			.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = MAX_TEXTURES
		},
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
	bufferInfos[2].buffer = m_renderDataBuffer;
	bufferInfos[2].range = VK_WHOLE_SIZE;
	bufferInfos[3].buffer = m_instanceDataBuffer;
	bufferInfos[3].range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet bufferWrites[6]{};
	for (int i = 0; i < 4; i++)
	{
		bufferWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		bufferWrites[i].dstSet = m_descriptorSet;
		bufferWrites[i].dstBinding = i;
		bufferWrites[i].descriptorCount = 1;
		bufferWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bufferWrites[i].pBufferInfo = &bufferInfos[i];
	}
	
	// Sampler
	VkDescriptorImageInfo samplerInfo{};
	samplerInfo.sampler = m_sampler;
	VkWriteDescriptorSet samplerWrite{};
	samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	samplerWrite.dstSet = m_descriptorSet;
	samplerWrite.dstBinding = 4;
	samplerWrite.descriptorCount = 1;
	samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	samplerWrite.pImageInfo = &samplerInfo;
	bufferWrites[4] = samplerWrite;

	// Texture (MUST BE LAST SINCE IT'S VARIABLE)
	std::vector<VkDescriptorImageInfo> imageInfos(m_textureImageViews.size());
	for (size_t i = 0; i < m_textureImageViews.size(); i++)
	{
		imageInfos[i].sampler = m_sampler;
		imageInfos[i].imageView = m_textureImageViews[i];
		imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet textureWrite{};
	textureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	textureWrite.dstSet = m_descriptorSet;
	textureWrite.dstBinding = 5;
	textureWrite.descriptorCount = static_cast<uint32_t>(imageInfos.size());
	textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureWrite.pImageInfo = imageInfos.data();
	bufferWrites[5] = textureWrite;
	

	vkUpdateDescriptorSets(VulkanCore::GetDevice(), 6, bufferWrites, 0, nullptr);

	Logger::Success("Created descriptor set");
}
