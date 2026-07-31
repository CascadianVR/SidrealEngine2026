#pragma once

// Shared pointer
#include <memory>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include "glm/glm.hpp"
#include <string>

struct Vertex {
    glm::vec4 position;
    glm::vec4 normal;
    glm::vec4 uv;
};

struct PushConstants {
	VkDeviceAddress vertexBufferDeviceAddress;
	VkDeviceAddress indexBufferDeviceAddress;
	glm::mat4 viewProjection;
	glm::mat4 model;
};

struct Mesh {
	std::vector<Vertex> vertices = std::vector<Vertex>();
	std::vector<uint32_t> indices = std::vector<uint32_t>();
	uint32_t vertexCount;
	uint32_t indexCount;
	uint32_t materialIndex;
	
	// Vulkan stuffs
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	VmaAllocation vertexBufferAllocation = VK_NULL_HANDLE;
	VmaAllocation indexBufferAllocation = VK_NULL_HANDLE;
	PushConstants pushConstants{};
};

struct Model {
	std::vector<Mesh> meshes = std::vector<Mesh>();
};

// Per-instance data
struct ModelInstance {
	std::shared_ptr<Model> model;
	glm::mat4 transform;
};

struct Pipeline {
	std::string fileName;
	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
};

struct ShaderData {
	glm::mat4 view;
	glm::mat4 projection;
	glm::mat4 viewProjection;
};

struct ShaderDataBuffer {
	VmaAllocation allocation{ VK_NULL_HANDLE };
	VmaAllocationInfo allocationInfo{};
	VkBuffer buffer{ VK_NULL_HANDLE };
	VkDeviceAddress deviceAddress{};
};

struct FrameResource {
	VkCommandPool commandPool{ VK_NULL_HANDLE };
	VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };
	VkSemaphore imageAquiredSemaphore{ VK_NULL_HANDLE };
};