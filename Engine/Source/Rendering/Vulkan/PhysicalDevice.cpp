#include "PhysicalDevice.h"
#include <Logger.h>
#include <utility>
#include <vector>

#include "VulkanCore.h"

PhysicalDevice::PhysicalDevice(const VkInstance& instance, const VkSurfaceKHR vulkanSurface)
{
	// Pick the first available physical device
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (deviceCount == 0)
	{
		throw std::runtime_error("Failed to find GPUs with Vulkan support");
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	m_physicalDevice = devices[0]; // Just pick the first one for simplicity
	
	// Check surface capabilities
	VkSurfaceCapabilitiesKHR capabilities;
	const VkResult capResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, vulkanSurface, &capabilities);
	if (capResult != VK_SUCCESS) {
		Logger::Error("Failed to get physical device surface capabilities");
		throw std::runtime_error("Failed to get physical device surface capabilities");
	}
	if (capabilities.minImageCount > VulkanCore::MinSwapChainImages)
	{
		Logger::Error("Surface does not support at least ", VulkanCore::MinSwapChainImages, " images! It supports at least ", capabilities.minImageCount);
	}
	
	// Get surface formats
	uint32_t formatCount = 0;
	VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, vulkanSurface, &formatCount, nullptr);

	if (result != VK_SUCCESS) {
		Logger::Error("Failed to get surface formats!");
	}

	if (formatCount == 0) {
		Logger::Error("No surface formats found!");
	}

	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, vulkanSurface, &formatCount, surfaceFormats.data());

	if (result != VK_SUCCESS) {
		Logger::Error("Failed to get surface formats!");
	}
	
	
	// Display selected physical device properties
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
	Logger::Info("Selected GPU: ", deviceProperties.deviceName);
	Logger::Info("Vulkan API Version: ", VK_VERSION_MAJOR(deviceProperties.apiVersion), ".",
		VK_VERSION_MINOR(deviceProperties.apiVersion), ".", VK_VERSION_PATCH(deviceProperties.apiVersion));

	// Get queue family properties
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

	uint32_t graphicsQueueFamilyIndex = -1;
	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
			// This Queue Family support graphics
			graphicsQueueFamilyIndex = i;
			Logger::Info("Graphics Queue Family Index: ", graphicsQueueFamilyIndex);
			break;
		}
	}

	if (std::cmp_equal(graphicsQueueFamilyIndex, -1)) {
		throw std::runtime_error("Failed to find a graphics queue family");
	}

	m_graphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
	
	Logger::Success("Physical device selected successfully!");
}
