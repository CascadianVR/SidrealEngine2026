#include "LogicalDevice.h"
#include <Logger.h>
#include <vector>

#include "VulkanCore.h"

LogicalDevice::LogicalDevice(PhysicalDevice& physicalDevice)
{
	float queuePriority = 1.0f;

	// Just make the queue create info for the graphics queue
	VkDeviceQueueCreateInfo queueCreateInfo{};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = physicalDevice.GetGraphicsQueueFamilyIndex();
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePriority;
	queueCreateInfo.pNext = nullptr;
	queueCreateInfo.flags = 0;

	std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
		VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
		VK_KHR_RAY_QUERY_EXTENSION_NAME
	};

	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
	accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	accelerationStructureFeatures.accelerationStructure = VK_TRUE;
	
	VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
	rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
	rayQueryFeatures.rayQuery = VK_TRUE;
	rayQueryFeatures.pNext = &accelerationStructureFeatures;

	VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
	rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	//rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
	rayTracingPipelineFeatures.pNext = &rayQueryFeatures;

	VkPhysicalDeviceVulkan11Features features11{};
	features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES; 
	features11.shaderDrawParameters = VK_TRUE;
	features11.pNext = &rayTracingPipelineFeatures;
	
	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.bufferDeviceAddress = VK_TRUE;
	features12.runtimeDescriptorArray = VK_TRUE;
	features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
	features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	features12.descriptorBindingPartiallyBound = VK_TRUE;
	features12.timelineSemaphore = VK_TRUE;
	features12.scalarBlockLayout = VK_TRUE;
	features12.pNext = &features11;

	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.synchronization2 = VK_TRUE;
	features13.dynamicRendering = VK_TRUE;
	features13.pNext = &features12;

	VkPhysicalDeviceFeatures2 features2{};
	features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	features2.features.samplerAnisotropy = VK_TRUE;
	features2.features.shaderInt64 = VK_TRUE;
	features2.pNext = &features13;

	// Create a logical device
	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pNext = &features2;
	createInfo.flags = 0;
	createInfo.queueCreateInfoCount = 1;
	createInfo.pQueueCreateInfos = &queueCreateInfo;
	createInfo.pEnabledFeatures = nullptr;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	if (vkCreateDevice(physicalDevice.GetPhysicalDevice(), &createInfo, nullptr, &m_logicalDevice) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create logical device");
	}
	
	Logger::Success("Logical device created successfully!");
}
