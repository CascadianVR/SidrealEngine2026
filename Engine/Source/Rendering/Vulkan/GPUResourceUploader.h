#pragma once

#include <vector>
#include <array>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "Rendering/RendererTypes.h"

class GPUResourceUploader {
public:
	static void CreateDataBuffers();
	static void CreateDescriptorSet();

	static std::vector<RenderData>& GetRenderData() { return m_renderData; }
	static VkDescriptorSetLayout* GetDescriptorSetLayout() { return &m_descriptorSetLayout; }
	static VkDescriptorSet* GetDescriptorSet() { return &m_descriptorSet; }
	static VkDeviceAddress GetVertexAddress() { return m_vertexBufferDeviceAddress; }
	static VkDeviceAddress GetIndexAddress() { return m_indexBufferDeviceAddress; }
	
	static constexpr int MAX_TEXTURES = 128;
	
	// Descriptor set bindings
	static constexpr int BINDING_COUNT = 7;
	static constexpr int VERTEX_BINDING = 0;
	static constexpr int INDEX_BINDING = 1;
	static constexpr int RENDER_DATA_BINDING = 2;
	static constexpr int INSTANCE_DATA_BINDING = 3;
	static constexpr int ACCELERATION_STRUCTURE_BINDING = 4;
	static constexpr int SAMPLER_BINDING = 5;
	static constexpr int TEXTURE_BINDING = 6;

private:
	static void CreateVertexBuffer(std::vector<Model>& models);
	static void CreateIndexBuffer(std::vector<Model>& models);
	static void CreateRenderDataBuffer(const std::vector<Model>& models);
	static void CreateInstanceDataBuffer(const std::vector<Model>& models);
	static void CreateTextureDataBuffer(const std::vector<Texture>& textures);
	static void CreateSampler();

	static inline std::vector<RenderData> m_renderData;
	static inline std::vector<InstanceData> m_instanceData;

	// Vulkan Stuffs
	static inline std::vector<VkDescriptorSetLayoutBinding> m_bindings;
	static inline std::array<VkDescriptorBindingFlags, BINDING_COUNT> m_bindingFlags{};
	static inline VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
	static inline VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	static inline VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
	
	static inline std::vector<VkImage> m_textureImages;
	static inline std::vector<VkImageView> m_textureImageViews;
	static inline std::vector<VmaAllocation> m_textureAllocations;
	static inline VkSampler m_sampler = VK_NULL_HANDLE;
	
	static inline VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
	static inline VkBuffer m_indexBuffer = VK_NULL_HANDLE;
	static inline VkBuffer m_renderDataBuffer = VK_NULL_HANDLE;
	static inline VkBuffer m_instanceDataBuffer = VK_NULL_HANDLE;
	static inline VmaAllocation m_vertexBufferAllocation = VK_NULL_HANDLE;
	static inline VmaAllocation m_indexBufferAllocation = VK_NULL_HANDLE;
	static inline VmaAllocation m_renderDataBufferAllocation = VK_NULL_HANDLE;
	static inline VmaAllocation m_instanceDataBufferAllocation = VK_NULL_HANDLE;
	static inline VkDeviceAddress m_vertexBufferDeviceAddress = 0;
	static inline VkDeviceAddress m_indexBufferDeviceAddress = 0;
	static inline VkDeviceAddress m_renderDataBufferDeviceAddress = 0;
	static inline VkDeviceAddress m_instanceDataBufferDeviceAddress = 0;
};
