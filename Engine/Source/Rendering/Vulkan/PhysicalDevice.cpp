#include "PhysicalDevice.h"
#include <Logger.h>
#include <utility>
#include <vector>

PhysicalDevice::PhysicalDevice(const VkInstance& instance)
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