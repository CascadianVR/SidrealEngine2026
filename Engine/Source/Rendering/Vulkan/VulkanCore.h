#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "Rendering/Vulkan/Swapchain.h"
#include "Rendering/Vulkan/PhysicalDevice.h"
#include "Rendering/Vulkan/LogicalDevice.h"
#include <vk_mem_alloc.h>
#include <vector>
#include <Rendering/RendererTypes.h>

class Window;

class VulkanCore {
public:
	static void Initialize(const Window* window, const std::vector<Model>& models);
	static void Render();
	static VmaAllocator GetAllocator() { return m_allocator; }
	static VkDevice GetDevice() { return m_logicalDevice.GetLogicalDevice(); }
	static constexpr uint32_t MaxFramesInFlight = 2;
	static constexpr int MAX_TEXTURES = 128;
private:

	static inline PhysicalDevice m_physicalDevice;
	static inline LogicalDevice m_logicalDevice;
	static inline Swapchain m_swapChain;
	static inline VmaAllocator m_allocator;

	static inline VkInstance m_instance = nullptr;
	static inline VkDebugUtilsMessengerEXT m_debugMessenger;
	static inline VkSurfaceKHR m_surface;
	static inline VkQueue m_queue;
	static inline std::vector<VkSemaphore> m_renderFinishedSemaphores;
	static inline uint32_t m_frameIndex;
	static inline uint64_t m_signalValue;
	static inline uint64_t m_waitValue;
	static inline uint64_t nextSignalValue = MaxFramesInFlight + 1;
	static inline bool m_requireSwapchainRecreate = false;
	static inline std::vector<FrameResource> m_frameResources;
	static inline VkSemaphore m_timelineSemaphore;
	static inline uint32_t m_apiVersion;

	// Descriptor and texture resources for the default texture used by shaders
	static inline VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
	static inline VkDescriptorSet m_descriptorSetTex = VK_NULL_HANDLE;
	static inline VkSampler m_defaultSampler = VK_NULL_HANDLE;
	static inline VkImage m_defaultTextureImage = VK_NULL_HANDLE;
	static inline VmaAllocation m_defaultTextureAllocation = VK_NULL_HANDLE;
	static inline VkImageView m_defaultTextureImageView = VK_NULL_HANDLE;

	static void CreateInstance();
	static void CreateDebugCallback();
	static void CreateSurface(const Window* window);
	static void CreateMemoryAllocator();
	static void CreateDescriptorResources();
	static void DestroyDescriptorResources();
	static void Shutdown();
	static void SetupDeviceQueueAndSemaphores();
	static void CreateCommandBuffers();
	static void CleanupSwapChain();
	static void RecreateSwapChain();
};