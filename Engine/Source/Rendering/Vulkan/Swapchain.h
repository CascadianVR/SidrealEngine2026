#pragma once


#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "Rendering/Vulkan/LogicalDevice.h"
#include "Rendering/Vulkan/PhysicalDevice.h"
#include <vk_mem_alloc.h>
#include <vector>

class Swapchain {
public:
	Swapchain() = default;
	Swapchain(VkSurfaceKHR& surface, PhysicalDevice& physicalDevice, LogicalDevice& logicalDevice, VmaAllocator& allocator, unsigned int width, unsigned int height);
	~Swapchain() = default;

	VkSwapchainKHR& GetSwapChain() { return m_swapChain; }
	VkImage GetSwapChainImage(uint32_t index) const { return m_swapChainImages[index]; }
	VkImageView GetSwapChainImageView(uint32_t index) const { return m_swapChainImageViews[index]; }
	VkImage GetDepthImage() const { return m_depthImage; }
	VkImageView GetDepthImageView() const { return m_depthImageView; }
	std::vector<VkImage>& GetSwapChainImages() { return m_swapChainImages; }
	uint32_t GetImageCount() const { return static_cast<uint32_t>(m_swapChainImages.size()); }
	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight() const { return m_height; }
	VkFormat GetDepthFormat() const { return m_depthFormat; }
	void DestroySwapChain();

private:
	VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
	VmaAllocator m_allocator = VK_NULL_HANDLE;
	std::vector<VkImage> m_swapChainImages = {};
	std::vector<VkImageView> m_swapChainImageViews = {};
	VkDevice m_device = VK_NULL_HANDLE;
	VkImage m_depthImage = VK_NULL_HANDLE;
	VkImageView m_depthImageView = VK_NULL_HANDLE;
	VmaAllocation m_depthImageAllocation = VK_NULL_HANDLE;
	VkFormat m_depthFormat;
	unsigned int m_width;
	unsigned int m_height;
};