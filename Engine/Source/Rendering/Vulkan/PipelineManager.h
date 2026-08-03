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
	static void Shutdown();
	static void CreatePipeline(const std::string& fileName, VkFormat depthFormat);
	static void CreateDescriptorSet();
	
	static Pipeline& GetPipeline(const std::string& shaderName) { return m_pipelines[shaderName]; }
	
private:
	static inline VkDevice m_device = VK_NULL_HANDLE;

	static inline Slang::ComPtr<slang::IGlobalSession> m_slangGlobalSession;
	static inline slang::SessionDesc m_slangSessionDesc{};
	static inline std::array<slang::TargetDesc, 1> m_slangTargets;
	static inline std::array<slang::CompilerOptionEntry, 1> m_slangOptions;
	
	static inline std::vector<ShaderDataBuffer> m_shaderDataBuffers;
	static inline std::unordered_map<std::string, Pipeline> m_pipelines;
	
	static inline std::vector<VkDescriptorSetLayoutBinding> m_bindings;
	static inline std::array<VkDescriptorBindingFlags, 5> m_bindingFlags{};
	static inline VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
	static inline VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	static inline VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
};