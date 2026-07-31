#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "Rendering/Vulkan/PhysicalDevice.h"

class LogicalDevice {
public:
	LogicalDevice() = default;
	explicit LogicalDevice(PhysicalDevice& physicalDevice);
	~LogicalDevice() = default;

	VkDevice& GetLogicalDevice() { return m_logicalDevice; }

private:
	VkDevice m_logicalDevice = VK_NULL_HANDLE;
};