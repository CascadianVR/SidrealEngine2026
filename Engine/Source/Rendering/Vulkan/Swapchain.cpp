#include "Swapchain.h"

#include <stdexcept>
#include "Logger.h"

Swapchain::Swapchain(VkSurfaceKHR& surface, PhysicalDevice& physicalDevice, LogicalDevice& logicalDevice, VmaAllocator& allocator, unsigned int width, unsigned int height)
{
	m_width = width;
	m_height = height;
	m_device = logicalDevice.GetLogicalDevice();
	m_allocator = allocator;
	uint32_t familyIndex = physicalDevice.GetGraphicsQueueFamilyIndex();

	VkSwapchainCreateInfoKHR swapChainCreateInfo {};
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.pNext = nullptr;
	swapChainCreateInfo.flags = 0;
	swapChainCreateInfo.surface = surface;
	swapChainCreateInfo.minImageCount = 3; // Triple buffering
	swapChainCreateInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
	swapChainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapChainCreateInfo.imageExtent = { width, height };
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapChainCreateInfo.queueFamilyIndexCount = 1;
	swapChainCreateInfo.pQueueFamilyIndices = &familyIndex;
	swapChainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(logicalDevice.GetLogicalDevice(), &swapChainCreateInfo, nullptr, &m_swapChain) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swap chain");
	}

	uint32_t imageCount = 0;
	vkGetSwapchainImagesKHR(logicalDevice.GetLogicalDevice(), m_swapChain, &imageCount, nullptr);

	// Get swap chain images
	std::vector<VkImage> swapChainImages(imageCount);
	vkGetSwapchainImagesKHR(logicalDevice.GetLogicalDevice(), m_swapChain, &imageCount, swapChainImages.data());

	// Create image views for each swap chain image
	m_swapChainImages.clear();
	m_swapChainImageViews.clear();
	m_swapChainImages.reserve(imageCount);
	m_swapChainImageViews.reserve(imageCount);
	for (uint32_t i = 0; i < imageCount; i++)
	{
		VkImageViewCreateInfo viewCreateInfo{};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.pNext = nullptr;
		viewCreateInfo.flags = 0;
		viewCreateInfo.image = swapChainImages[i];
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
		viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCreateInfo.subresourceRange.baseMipLevel = 0;
		viewCreateInfo.subresourceRange.levelCount = 1;
		viewCreateInfo.subresourceRange.baseArrayLayer = 0;
		viewCreateInfo.subresourceRange.layerCount = 1;

		// Create image view for each swap chain image
		VkImageView imageView;
		if (vkCreateImageView(logicalDevice.GetLogicalDevice(), &viewCreateInfo, nullptr, &imageView) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create image views for swap chain");
		}

		m_swapChainImages.push_back(swapChainImages[i]);
		m_swapChainImageViews.push_back(imageView);
	}

	std::vector<VkFormat> depthFormatList{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT };
	VkFormat depthFormat = VK_FORMAT_UNDEFINED;
	for (VkFormat& format : depthFormatList) {
		VkFormatProperties2 formatProperties{};
		formatProperties.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2;
		vkGetPhysicalDeviceFormatProperties2(physicalDevice.GetPhysicalDevice(), format, &formatProperties);
		if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
			depthFormat = format;
			break;
		}
	}
	m_depthFormat = depthFormat;

	Logger::Info("Depth format selected : ", depthFormat);

	// Create depth image
	VkImageCreateInfo depthImageCreateInfo {};
	depthImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	depthImageCreateInfo.format = m_depthFormat;
	depthImageCreateInfo.extent = { .width = static_cast<uint32_t>(width), .height = static_cast<uint32_t>(height), .depth = 1 };
	depthImageCreateInfo.mipLevels = 1;
	depthImageCreateInfo.arrayLayers = 1;
	depthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	depthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocatorCreateInfo {};
	allocatorCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	allocatorCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

	if (vmaCreateImage(m_allocator, &depthImageCreateInfo, &allocatorCreateInfo, &m_depthImage, &m_depthImageAllocation, nullptr) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create depth image");
	}

	// Create depth image view
	VkImageAspectFlags depthAspects = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (m_depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || m_depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) depthAspects |= VK_IMAGE_ASPECT_STENCIL_BIT;

	VkImageViewCreateInfo depthViewCreateInfo{};
	depthViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthViewCreateInfo.image = m_depthImage;
	depthViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthViewCreateInfo.format = m_depthFormat;
	depthViewCreateInfo.subresourceRange = { .aspectMask = depthAspects, .levelCount = 1, .layerCount = 1 };
	
	if (vkCreateImageView(m_device, &depthViewCreateInfo, nullptr, &m_depthImageView) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create depth image view");
	}

	Logger::Success("Swap chain created successfully!");
}

void Swapchain::DestroySwapChain()
{
	for (VkImageView view : m_swapChainImageViews)
	{
		vkDestroyImageView(m_device, view, nullptr);
	}
	m_swapChainImageViews.clear();

	if (m_swapChain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
		m_swapChain = VK_NULL_HANDLE;
	}

	// We do not destroy the swap chain images here as they are managed by Vulkan and
	// will be cleaned up when the swap chain is destroyed
	m_swapChainImages.clear();

	// Clean up depth image and its allocation
	if (m_depthImage != VK_NULL_HANDLE)
	{
		vmaDestroyImage(m_allocator, m_depthImage, m_depthImageAllocation);
		m_depthImage = VK_NULL_HANDLE;
		m_depthImageAllocation = VK_NULL_HANDLE;
	}

	// Cleanup depth image view
	if (m_depthImageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(m_device, m_depthImageView, nullptr);
		m_depthImageView = VK_NULL_HANDLE;
	}

	// Clean up the allocator reference
	m_allocator = VK_NULL_HANDLE;

	Logger::Info("Swap chain destroyed.");

}