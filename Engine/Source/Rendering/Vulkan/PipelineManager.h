#pragma once
#include <string>
#include <vulkan/vulkan.h>
#include <slang/slang.h>
#include <slang/slang-com-ptr.h>
#include <array>
#include <unordered_map>
#include "Rendering/RendererTypes.h"

class PipelineManager {
public:
	static void Initialize(const VkDevice& device);
	static void CreatePipeline(const std::string& fileName, VkFormat depthFormat);
	static Pipeline& GetPipeline(const std::string& shaderName) { return m_pipelines[shaderName]; }
	static void Shutdown();
	
private:
	static inline VkDevice m_device = VK_NULL_HANDLE;

	static inline Slang::ComPtr<slang::IGlobalSession> m_slangGlobalSession;
	static inline slang::SessionDesc m_slangSessionDesc{};
	static inline std::array<slang::TargetDesc, 1> m_slangTargets;
	static inline std::array<slang::CompilerOptionEntry, 1> m_slangOptions;

	static inline std::vector<ShaderDataBuffer> m_shaderDataBuffers;
	static inline VkBuffer m_vertexBuffer{ VK_NULL_HANDLE };
	static inline VkDeviceAddress m_vertexBufferDeviceAddress{};
	static inline VmaAllocationInfo m_vertexBufferAllocationInfo{};
	static inline VmaAllocation m_vertexBufferAllocation{ VK_NULL_HANDLE };
	
	static inline std::unordered_map<std::string, Pipeline> m_pipelines;
};