#pragma once

#include <vector>
#include <array>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "Rendering/RendererTypes.h"

class GPUResourceUploader {
public:
	static void CreateDataBuffers(const std::vector<Model>& models);

	static std::vector<RenderData>& GetRenderData() { return m_renderData; }
	static PushConstants& GetPushConstants() { return m_pushConstants; }
	static VkDescriptorSetLayout* GetDescriptorSetLayout() { return &m_descriptorSetLayout; }
	static VkDescriptorSet* GetDescriptorSet() { return &m_descriptorSet; }
	static constexpr int MAX_TEXTURES = 128;

private:
	static void CreateVertexBuffer(const std::vector<Model>& models);
	static void CreateIndexBuffer(const std::vector<Model>& models);
	static void CreateRenderDataBuffer(const std::vector<Model>& models);
	static void CreateInstanceDataBuffer(const std::vector<Model>& models);
	static void CreateDescriptorSet();

	static inline std::vector<RenderData> m_renderData;
	static inline std::vector<InstanceData> m_instanceData;

	// Vulkan Stuffs
	static inline std::vector<VkDescriptorSetLayoutBinding> m_bindings;
	static inline std::array<VkDescriptorBindingFlags, 5> m_bindingFlags{};
	static inline VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
	static inline VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	static inline VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

	static inline VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
	static inline VkBuffer m_indexBuffer = VK_NULL_HANDLE;
	static inline VkBuffer m_renderDataBuffer = VK_NULL_HANDLE;
	static inline VkBuffer dataBuffer = VK_NULL_HANDLE;
	static inline VmaAllocation m_vertexBufferAllocation = VK_NULL_HANDLE;
	static inline VmaAllocation m_indexBufferAllocation = VK_NULL_HANDLE;
	static inline VmaAllocation m_renderDataBufferAllocation = VK_NULL_HANDLE;
	static inline VmaAllocation m_instanceDataBufferAllocation = VK_NULL_HANDLE;
	static inline PushConstants m_pushConstants{};
};
