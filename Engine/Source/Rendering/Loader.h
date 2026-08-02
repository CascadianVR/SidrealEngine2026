#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include "Rendering/RendererTypes.h"
#include "tiny_gltf_v3.h"

class Loader {
public:
	static void LoadAllAssets();
	static std::unordered_map<std::string, Model>& GetLoadedModels() { return m_loadedModels; }
	static PushConstants& GetPushConstants() { return m_pushConstants; }
private:
	static void LoadGLB(const std::string& fileName, Model& model);
	static void GetVertexData(const tg3_model& model, const tg3_primitive& primitive, Mesh& mesh);
	static void GetIndexData(const tg3_model& model, uint32_t accessorIndex, Mesh& mesh);
	static void CreateVertexBuffer();
	static void CreateIndexBuffer();
	static int GetAccessorIndex(const tg3_primitive &primitive, const char* accessorName);
	static inline std::unordered_map<std::string, Model> m_loadedModels = { { "Resources/Models/Cascadia.glb", Model() } };
	
	// Vulkan Stuffs
	static inline VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
	static inline VkBuffer m_indexBuffer = VK_NULL_HANDLE;
	static inline VmaAllocation m_vertexBufferAllocation = VK_NULL_HANDLE;
	static inline VmaAllocation m_indexBufferAllocation = VK_NULL_HANDLE;
	static inline PushConstants m_pushConstants{};
};