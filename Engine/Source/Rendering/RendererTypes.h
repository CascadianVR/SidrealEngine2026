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

struct Texture
{
	int32_t width;
	int32_t height;
	uint32_t channels; 
	std::vector<uint8_t> pixels;
};

struct Mesh {
	std::vector<Vertex> vertices = std::vector<Vertex>();
	std::vector<uint32_t> indices = std::vector<uint32_t>();
	uint32_t vertexCount;
	uint32_t indexCount;
	uint32_t vertexOffset;
	uint32_t indexOffset;
	uint32_t textureIndex = 0;
};

struct Model {
	std::string name;
	std::vector<Mesh> meshes = std::vector<Mesh>();
	std::vector<glm::mat4> instanceMatrices = std::vector( { glm::mat4(1.0f) } );
	uint32_t instanceCount = 1;
	uint32_t instanceOffset;
};

// Per-Instance Data
struct InstanceData {
	glm::mat4 modelMatrix; // Transform of this instance
};

// Per-Mesh Render Data
struct RenderData
{
	uint32_t vertexOffset;   // Offset in the vertex buffer
	uint32_t indexOffset;    // Offset in the index buffer
	uint32_t indexCount;     // Number of indices in this mesh
	uint32_t instanceOffset; // Offset in the instance buffer
	uint32_t instanceCount;  // Number of instances to render
	uint32_t textureIndex;   // Index of the texture to use
};

struct Pipeline {
	std::string fileName;
	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
};

struct PushConstants {
	glm::mat4 viewProjection;
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