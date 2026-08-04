#define VMA_IMPLEMENTATION
#include "VulkanCore.h"

#include <SDL_vulkan.h>
#include <iostream>
#include <vector>
#include <array>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/glm.hpp>

#include "Input.h"
#include "Application.h"
#include "Window.h"
#include "Logger.h"
#include "Rendering/Vulkan/GPUResourceUploader.h"
#include "Rendering/Vulkan/PipelineManager.h"
#include "Rendering/Vulkan/RayTracing/AccelerationStructure.h"

float yaw = -90.0f;
float pitch = -10.0f;
	
glm::vec3 cameraPos   = glm::vec3(0.0f, 1.3f, 2.5f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);

void VulkanCore::Initialize(const Window* window)
{
	CreateInstance();
	CreateDebugCallback();
	CreateSurface(window);
	m_physicalDevice = PhysicalDevice(m_instance);
	m_logicalDevice = LogicalDevice(m_physicalDevice);
	CreateMemoryAllocator(); // Need instance and both devices to create allocator
	m_swapChain = Swapchain(m_surface, m_physicalDevice, m_logicalDevice, m_allocator, window->GetWidth(), window->GetHeight());
	SetupDeviceQueueAndSemaphores();
	CreateCommandBuffers();

	GPUResourceUploader::CreateDataBuffers(); // Create before pipeline
	
	AccelerationStructure::CreateBLASForMeshes();
	AccelerationStructure::CreateHLASForMeshes();
	AccelerationStructure::BuildAccelerationStructures();
	
	PipelineManager::Initialize(m_logicalDevice.GetLogicalDevice());
	PipelineManager::CreatePipeline("Resources/Shaders/shader2.slang", m_swapChain.GetDepthFormat());
}

void VulkanCore::Render()
{
	Window* window = Application::GetWindow();
	// Skip rendering if the window doesn't have anything to render
	if (window->GetWidth() == 0 || window->GetHeight() == 0)
	{
		return;
	}

	// If window size has changed or we require swapchain recreation for some other reason, do it
	if (m_requireSwapchainRecreate || m_swapChain.GetWidth() != window->GetWidth() || m_swapChain.GetHeight() != window->GetHeight())
	{
		Logger::Warn("Recreating swapchain");
		RecreateSwapChain();
	}


	m_frameIndex = (m_frameIndex + 1) % MaxFramesInFlight;
	const uint64_t signalValue = nextSignalValue++;
	const uint64_t waitValue = signalValue - MaxFramesInFlight;

	VkSemaphoreWaitInfo waitCreateInfo{};
	waitCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
	waitCreateInfo.semaphoreCount = 1;
	waitCreateInfo.pSemaphores = &m_timelineSemaphore;
	waitCreateInfo.pValues = &waitValue;

	vkWaitSemaphores(m_logicalDevice.GetLogicalDevice(), &waitCreateInfo, UINT64_MAX);

	FrameResource frameResource = m_frameResources[m_frameIndex];
	VkSemaphore imageAquireSemaphore = frameResource.imageAquiredSemaphore;

	uint32_t imageIndex = 0;
	VkResult result = vkAcquireNextImageKHR(m_logicalDevice.GetLogicalDevice(), m_swapChain.GetSwapChain(), UINT64_MAX, imageAquireSemaphore, VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		m_requireSwapchainRecreate = true;
		return;
	}
	else if (result == VK_SUBOPTIMAL_KHR)
	{
		m_requireSwapchainRecreate = true;
	}

	VkCommandBufferBeginInfo commandBufferBeginInfo{};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(frameResource.commandBuffer, &commandBufferBeginInfo);

	VkImageAspectFlags depthAspects = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (m_swapChain.GetDepthFormat() == VK_FORMAT_D32_SFLOAT_S8_UINT ||
		m_swapChain.GetDepthFormat() == VK_FORMAT_D24_UNORM_S8_UINT)
	{
		depthAspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	std::array<VkImageMemoryBarrier2, 2> outputBarriers{
		VkImageMemoryBarrier2{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.image = m_swapChain.GetSwapChainImage(imageIndex),
			.subresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 }
		},
		VkImageMemoryBarrier2{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
			.srcAccessMask = 0,
			.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
			.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
			.image = m_swapChain.GetDepthImage(),
			.subresourceRange{.aspectMask = depthAspects, .levelCount = 1, .layerCount = 1 }
		}
	};

	VkDependencyInfo barrierDependencyInfo{};
	barrierDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	barrierDependencyInfo.imageMemoryBarrierCount = 2;
	barrierDependencyInfo.pImageMemoryBarriers = outputBarriers.data();

	vkCmdPipelineBarrier2(frameResource.commandBuffer, &barrierDependencyInfo);

	VkRenderingAttachmentInfo colorAttachmentInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = m_swapChain.GetSwapChainImageView(imageIndex),
		.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue{.color{ { 0.0f, 0.5f, 0.5f, 1.0f } }}
	};
	VkRenderingAttachmentInfo depthAttachmentInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.imageView = m_swapChain.GetDepthImageView(),
		.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
		.clearValue = {.depthStencil = {.depth = 1.0f,  .stencil = 0}}
	};

	VkRenderingInfo renderingInfo{
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea{
			.offset
			{
				.x = 0,
				.y = 0
			},
			.extent
			{
				.width = static_cast<uint32_t>(m_swapChain.GetWidth()),
				.height = static_cast<uint32_t>(m_swapChain.GetHeight())
			}
		},
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &colorAttachmentInfo,
		.pDepthAttachment = &depthAttachmentInfo,
	};
	
	Pipeline& pipeline = PipelineManager::GetPipeline("Resources/Shaders/shader2.slang");
	
	// Camera look
	yaw += static_cast<float>(Input::deltaMouseX) * 0.1f;
	pitch += -static_cast<float>(Input::deltaMouseY) * 0.1f;
	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(front);
	
	// Camera Move
	constexpr float speed = 0.015f;
	glm::vec3 cameraOffset{ 0.0f, 0.0f, 0.0f };
	glm::vec3 right =
	glm::normalize(glm::cross(cameraFront, glm::vec3(0, 1, 0)));
	glm::vec3 up = glm::vec3(0, 1, 0);
	if (Input::wKeyPressed) cameraOffset += cameraFront;
	if (Input::sKeyPressed) cameraOffset -= cameraFront;
	if (Input::aKeyPressed) cameraOffset -= right;
	if (Input::dKeyPressed) cameraOffset += right;
	if (Input::qKeyPressed) cameraOffset -= up;
	if (Input::eKeyPressed) cameraOffset += up;
	if (glm::length(cameraOffset) > 0.0f) cameraPos += glm::normalize(cameraOffset) * speed;
	
	// Calculate view and projection
	glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
	glm::mat4 projection = glm::perspectiveFov(glm::radians(45.0f), static_cast<float>(window->GetWidth()), static_cast<float>(window->GetHeight()), 0.1f, 100.0f);

	// Bind descriptor sets
	vkCmdBindDescriptorSets(
		frameResource.commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipeline.pipelineLayout,
		0, 1, GPUResourceUploader::GetDescriptorSet(),
		0, nullptr
	);
	
	// Push constants for camera
	PushConstants pushConstants{};
	pushConstants.viewProjection = projection * view;
	vkCmdPushConstants(frameResource.commandBuffer, pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pushConstants);
	
	vkCmdBeginRendering(frameResource.commandBuffer, &renderingInfo);

	VkViewport viewport{};
	viewport.x = 0;
	viewport.y = static_cast<float>(window->GetHeight());
	viewport.width = static_cast<float>(window->GetWidth());
	viewport.height = -static_cast<float>(window->GetHeight());
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(frameResource.commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent = { 
		.width = window->GetWidth(),
		.height = window->GetHeight()
	};
	vkCmdSetScissor(frameResource.commandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(frameResource.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

	// Draw each mesh
	std::vector<RenderData> renderData = GPUResourceUploader::GetRenderData();
	for (size_t i = 0; i < renderData.size(); i++)
	{
		RenderData& data = renderData[i];
		vkCmdDraw(frameResource.commandBuffer, data.indexCount, data.instanceCount, 0, static_cast<uint32_t>(i));
	}
	
	vkCmdEndRendering(frameResource.commandBuffer);

	VkImageMemoryBarrier2 barrierPresent{};
	barrierPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrierPresent.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	barrierPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	barrierPresent.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
	barrierPresent.dstAccessMask = 0;
	barrierPresent.oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	barrierPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrierPresent.image = m_swapChain.GetSwapChainImage(imageIndex);
	barrierPresent.subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1 };

	VkDependencyInfo barrierPresentDependencyInfo{};
	barrierPresentDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	barrierPresentDependencyInfo.imageMemoryBarrierCount = 1;
	barrierPresentDependencyInfo.pImageMemoryBarriers = &barrierPresent;

	vkCmdPipelineBarrier2(frameResource.commandBuffer, &barrierPresentDependencyInfo);
	
	if (vkEndCommandBuffer(frameResource.commandBuffer) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to record command buffer");
	}

	VkSemaphoreSubmitInfo semaphoreAcquireWaitInfo{};
	semaphoreAcquireWaitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	semaphoreAcquireWaitInfo.semaphore = imageAquireSemaphore;
	semaphoreAcquireWaitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	std::array<VkSemaphoreSubmitInfo, 2> semaphoreSignals{
		VkSemaphoreSubmitInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = m_renderFinishedSemaphores[imageIndex],
			.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
		},
		VkSemaphoreSubmitInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = m_timelineSemaphore,
			.value = signalValue,
			.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
		}
	};

	VkCommandBufferSubmitInfo commandBufferSubmitInfo{};
	commandBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandBufferSubmitInfo.commandBuffer = frameResource.commandBuffer;

	VkSubmitInfo2 submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.waitSemaphoreInfoCount = 1;
	submitInfo.pWaitSemaphoreInfos = &semaphoreAcquireWaitInfo;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &commandBufferSubmitInfo;
	submitInfo.signalSemaphoreInfoCount = static_cast<uint32_t>(semaphoreSignals.size());
	submitInfo.pSignalSemaphoreInfos = semaphoreSignals.data();

	vkQueueSubmit2(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[imageIndex];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_swapChain.GetSwapChain();
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	vkQueuePresentKHR(m_queue, &presentInfo);
}

void VulkanCore::CreateInstance()
{
	// Setup Vulkan instance
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Learning App";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Sidreal Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	vkEnumerateInstanceVersion(&m_apiVersion);
	appInfo.apiVersion = m_apiVersion;
	
	if (m_apiVersion < VK_API_VERSION_1_3)
	{
		Logger::Error("Vulkan API version is too low!");
		throw std::runtime_error("Vulkan API version is too low!");
	}
	
	// Get SDL extensions
	unsigned int sdlExtensionCount = 0;
	if (!SDL_Vulkan_GetInstanceExtensions(Application::GetWindow()->GetSDLWindow(), &sdlExtensionCount, nullptr)) {
		Logger::Error("Failed to retrieve the number of required Vulkan extensions: ", SDL_GetError());
	}
	std::vector<const char*> extensions(sdlExtensionCount);
	if (!SDL_Vulkan_GetInstanceExtensions(Application::GetWindow()->GetSDLWindow(), &sdlExtensionCount, extensions.data())) {
		Logger::Error("Failed to retrieve the required Vulkan extensions: ", SDL_GetError());
	}
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	
	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	// List extensions being used
	std::cout << "Available Vulkan extensions:\n";
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
	for (const auto& extension : availableExtensions)
	{
		std::cout << "\t" << extension.extensionName << "\n";
	}

	// Validation layers
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};
	createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
	createInfo.ppEnabledLayerNames = validationLayers.data();

	if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Vulkan instance");
	}

	// Display current Vulkan API version and extensions
	Logger::Info("Vulkan API Version: ", VK_VERSION_MAJOR(m_apiVersion), ".",
		VK_VERSION_MINOR(m_apiVersion), ".", VK_VERSION_PATCH(m_apiVersion));

	Logger::Success("Vulkan instance created successfully!");
}

void VulkanCore::CreateDebugCallback()
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = [](
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData) -> VkBool32
	{
		Logger::Error("Validation Layer: ", pCallbackData->pMessage);
		return VK_FALSE;
	};

	const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
	if (func != nullptr)
	{
		if (func(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to set up debug messenger");
		}
		Logger::Success("Debug callback created successfully!");
	}
	else
	{
		throw std::runtime_error("Failed to get vkCreateDebugUtilsMessengerEXT function");
	}
}

void VulkanCore::CreateSurface(const Window* window)
{
	if (m_instance == VK_NULL_HANDLE) {
		throw std::runtime_error("Vulkan instance is null before CreateSurface");
	}
	Logger::Info("Creating surface with instance: ", m_instance);

	const SDL_bool success = SDL_Vulkan_CreateSurface(window->GetSDLWindow(), m_instance, &m_surface);

	// Create a Vulkan surface for the SDL window
	if (!success)
	{
		throw std::runtime_error("Failed to create window surface");
	}

	Logger::Success("Vulkan surface created successfully!");
}

void VulkanCore::CreateMemoryAllocator()
{
	// Setup Vulkan Memory Allocator (VMA)
	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.vulkanApiVersion = m_apiVersion; // Match your target API version
	allocatorInfo.instance = m_instance;
	allocatorInfo.physicalDevice = m_physicalDevice.GetPhysicalDevice();
	allocatorInfo.device = m_logicalDevice.GetLogicalDevice();
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT; // Enable buffer device address support

	const VkResult result = vmaCreateAllocator(&allocatorInfo, &m_allocator);
	if (result != VK_SUCCESS) {
		Logger::Error("Failed to create Vulkan Memory Allocator (VMA)");
	}

	Logger::Success("Vulkan Memory Allocator (VMA) created successfully!");
}

void VulkanCore::CreateCommandBuffers()
{
	for (FrameResource& frameResource : m_frameResources)
	{
		VkCommandPoolCreateInfo poolCreateInfo{};
		poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolCreateInfo.pNext = nullptr;
		poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolCreateInfo.queueFamilyIndex = m_physicalDevice.GetGraphicsQueueFamilyIndex();

		if (vkCreateCommandPool(m_logicalDevice.GetLogicalDevice(), &poolCreateInfo, nullptr, &frameResource.commandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create command pool");
		}

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = frameResource.commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(m_logicalDevice.GetLogicalDevice(), &allocInfo, &frameResource.commandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to allocate command buffer");
		}
	}

	Logger::Success("Command buffers created successfully!");
}

void VulkanCore::SetupDeviceQueueAndSemaphores()
{
	vkGetDeviceQueue(m_logicalDevice.GetLogicalDevice(), m_physicalDevice.GetGraphicsQueueFamilyIndex(), 0, &m_queue);

	Logger::Success("Device queue set up successfully!");

	VkSemaphoreTypeCreateInfo timelineSemaphoreTypeInfo{};
	timelineSemaphoreTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	timelineSemaphoreTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timelineSemaphoreTypeInfo.initialValue = MaxFramesInFlight;

	// Create timeline semaphore
	VkSemaphoreCreateInfo timelineSemaphoreInfo{};
	timelineSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	timelineSemaphoreInfo.pNext = &timelineSemaphoreTypeInfo;

	if (vkCreateSemaphore(m_logicalDevice.GetLogicalDevice(), &timelineSemaphoreInfo, nullptr, &m_timelineSemaphore) != VK_SUCCESS)
	{
		Logger::Error("Failed to create timeline semaphore");
	}

	// Per-frame semaphores for image availability and render finished
	
	m_frameResources.clear();
	m_frameResources.resize(MaxFramesInFlight);
	for (uint32_t i = 0; i < MaxFramesInFlight; i++)
	{
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(m_logicalDevice.GetLogicalDevice(), &semaphoreInfo, nullptr, &m_frameResources[i].imageAquiredSemaphore) != VK_SUCCESS)
		{
			Logger::Error("Failed to create image available semaphore");
		}
	}

	m_renderFinishedSemaphores.clear();
	m_renderFinishedSemaphores.resize(m_swapChain.GetImageCount());
	for (VkSemaphore& semaphore : m_renderFinishedSemaphores) {
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(m_logicalDevice.GetLogicalDevice(), &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
		{
			Logger::Error("Failed to create render finished semaphore");
		}
	}

	Logger::Success("Device queue and semaphores set up successfully!");
}

void VulkanCore::RecreateSwapChain()
{
	vkDeviceWaitIdle(m_logicalDevice.GetLogicalDevice());

	// If window is minimized, wait until it's restored
	int width = 0, height = 0;
	if (Application::GetWindow())
	{
		while (width == 0 || height == 0)
		{
			SDL_GetWindowSize(Application::GetWindow()->GetSDLWindow(), &width, &height);
		}
	}

	Logger::Info("Recreating swapchain with dimensions: ", width, " x ", height);
	
	// Wait for device idle and cleanup old swapchain-dependent resources
	CleanupSwapChain();

	// Recreate swapchain (construct new Swapchain object)
	m_swapChain.DestroySwapChain();
	m_swapChain = Swapchain(m_surface, m_physicalDevice, m_logicalDevice, m_allocator, width, height);

	// Prepare per-frame resources and recreate command buffers for the new swapchain image count
	m_frameResources.clear();
	m_frameResources.resize(MaxFramesInFlight);

	// Recreate command pools and command buffers for each frame resource
	CreateCommandBuffers();

	// Recreate per-frame semaphores (image acquired)
	for (auto &m_frameResource : m_frameResources) {
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(m_logicalDevice.GetLogicalDevice(), &semaphoreInfo, nullptr, &m_frameResource.imageAquiredSemaphore) != VK_SUCCESS)
		{
			Logger::Error("Failed to create image available semaphore during swapchain recreation");
		}
	}

	// Recreate per-swapchain-image render-finished semaphores
	m_renderFinishedSemaphores.clear();
	m_renderFinishedSemaphores.resize(m_swapChain.GetImageCount());
	for (VkSemaphore& semaphore : m_renderFinishedSemaphores) {
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(m_logicalDevice.GetLogicalDevice(), &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
		{
			Logger::Error("Failed to create render finished semaphore during swapchain recreation");
		}
	}
}

void VulkanCore::CleanupSwapChain()
{
	for (auto frameResource : m_frameResources)
	{
		vkDestroySemaphore(m_logicalDevice.GetLogicalDevice(), frameResource.imageAquiredSemaphore, nullptr);
		if (frameResource.commandPool != VK_NULL_HANDLE) {
			vkFreeCommandBuffers(m_logicalDevice.GetLogicalDevice(), frameResource.commandPool, 1, &frameResource.commandBuffer);
			vkDestroyCommandPool(m_logicalDevice.GetLogicalDevice(), frameResource.commandPool, nullptr);
		}
	}

	for (auto semaphore : m_renderFinishedSemaphores)
	{
		if (semaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(m_logicalDevice.GetLogicalDevice(), semaphore, nullptr);
		}
	}

	m_renderFinishedSemaphores.clear();
	m_frameResources.clear();
}

void VulkanCore::Shutdown()
{
	VkDevice device = m_logicalDevice.GetLogicalDevice();
	// Wait until device is idle
	if (device != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(device);
	}

	// Cleanup swapchain related resources
	CleanupSwapChain();

	// Shutdown shader resources
	PipelineManager::Shutdown();

	// Destroy timeline semaphore
	if (m_timelineSemaphore != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
		vkDestroySemaphore(device, m_timelineSemaphore, nullptr);
		m_timelineSemaphore = VK_NULL_HANDLE;
	}

	// Destroy any remaining render finished semaphores
	for (auto sem : m_renderFinishedSemaphores) {
		if (sem != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
			vkDestroySemaphore(device, sem, nullptr);
		}
	}
	m_renderFinishedSemaphores.clear();

	// Destroy allocator
	if (m_allocator != VK_NULL_HANDLE) {
		vmaDestroyAllocator(m_allocator);
		m_allocator = VK_NULL_HANDLE;
	}

	// Shutdown logical device
	if (device != VK_NULL_HANDLE) {
		vkDestroyDevice(device, nullptr);
	}

	// Destroy surface
	if (m_surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
		m_surface = VK_NULL_HANDLE;
	}

	// Destroy debug messenger
	if (m_debugMessenger != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
		const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
		if (func) func(m_instance, m_debugMessenger, nullptr);
		m_debugMessenger = VK_NULL_HANDLE;
	}

	// Destroy instance
	if (m_instance != VK_NULL_HANDLE) {
		vkDestroyInstance(m_instance, nullptr);
		m_instance = VK_NULL_HANDLE;
	}
}
