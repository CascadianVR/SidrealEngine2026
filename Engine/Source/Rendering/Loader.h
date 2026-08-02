#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "Rendering/RendererTypes.h"
#include "tiny_gltf_v3.h"

class Loader {
public:
	static void LoadScene(const std::string& fileName);
	static std::vector<Model>& GetLoadedModels() { return m_loadedModels; }
	static PushConstants& GetPushConstants() { return m_pushConstants; }
	static std::vector<RenderData>& GetRenderData() { return m_renderData; }
private:
	static void CreateDataBuffers();
	static void LoadGLB(const std::string& fileName, glm::mat4& modelMatrix);
	static void GetVertexData(const tg3_model& model, const tg3_primitive& primitive, Mesh& mesh);
	static void GetIndexData(const tg3_model& model, uint32_t accessorIndex, Mesh& mesh);
	static void CreateVertexBuffer();
	static void CreateIndexBuffer();
	static void CreateRenderDataBuffer();
	static void CreateInstanceDataBuffer();
	static int GetAccessorIndex(const tg3_primitive &primitive, const char* accessorName);
	
	static inline std::vector<Model> m_loadedModels;
	static inline std::unordered_map<std::string, uint32_t> m_modelAssetLookup;
	static inline std::vector<RenderData> m_renderData;
	static inline std::vector<InstanceData> m_instanceData;

	// Vulkan Stuffs
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