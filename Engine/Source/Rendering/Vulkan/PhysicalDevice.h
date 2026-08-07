#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

class PhysicalDevice {
public:
	PhysicalDevice() = default;
	PhysicalDevice(const VkInstance& instance, const VkSurfaceKHR vulkanSurface);
	~PhysicalDevice() = default;

	VkPhysicalDevice& GetPhysicalDevice() { return m_physicalDevice; }
	uint32_t GetGraphicsQueueFamilyIndex() const { return m_graphicsQueueFamilyIndex; }

private:
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	uint32_t m_graphicsQueueFamilyIndex = -1;
};