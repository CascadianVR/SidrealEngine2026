#include "PipelineManager.h"
#include <vector>
#include <stdexcept>
#include <Logger.h>
#include <ranges>
#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;

#include "Rendering/RendererTypes.h"
#include "Rendering/Vulkan/GPUResourceUploader.h"
#include "Rendering/Vulkan/VulkanCore.h"

void PipelineManager::Initialize(const VkDevice& device)
{
	m_device = device;

	// Initialize Slang
	slang::createGlobalSession(m_slangGlobalSession.writeRef());
	m_slangTargets = std::to_array<slang::TargetDesc>({ {.format{SLANG_SPIRV}, .profile{ m_slangGlobalSession->findProfile("spirv_1_4")} } });
	m_slangOptions = std::to_array<slang::CompilerOptionEntry>({ { .name = slang::CompilerOptionName::EmitSpirvDirectly, .value = {.kind = slang::CompilerOptionValueKind::Int, .intValue0 = 1} } });

	m_slangSessionDesc = {};
	m_slangSessionDesc.targets = { m_slangTargets.data() };
	m_slangSessionDesc.targetCount = { static_cast<SlangInt>(m_slangTargets.size()) };
	m_slangSessionDesc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
	m_slangSessionDesc.compilerOptionEntries = { m_slangOptions.data() };
	m_slangSessionDesc.compilerOptionEntryCount = { static_cast<uint32_t>(m_slangOptions.size()) };

	Logger::Success("Slang initialized!");

	// Create shader data buffers
	m_shaderDataBuffers.resize(VulkanCore::MaxFramesInFlight);
	for (uint32_t i = 0; i < VulkanCore::MaxFramesInFlight; i++) {
		VkBufferCreateInfo uniformBufferCreateInfo{};
		uniformBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		uniformBufferCreateInfo.size = sizeof(ShaderData);
		uniformBufferCreateInfo.usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

		VmaAllocationCreateInfo uBufferAllocCreateInfo{};
		uBufferAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		uBufferAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

		if (vmaCreateBuffer(VulkanCore::GetAllocator(), &uniformBufferCreateInfo, &uBufferAllocCreateInfo, &m_shaderDataBuffers[i].buffer, &m_shaderDataBuffers[i].allocation, &m_shaderDataBuffers[i].allocationInfo) != VK_SUCCESS)
		{
			Logger::Error("Failed to create shader data buffer for frame ", i);
		}

		VkBufferDeviceAddressInfo uniformBufferDeviceAddressInfo{};
		uniformBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
		uniformBufferDeviceAddressInfo.buffer = m_shaderDataBuffers[i].buffer;

		m_shaderDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(device, &uniformBufferDeviceAddressInfo);
	}
	Logger::Success("Shader data buffers created!");
}

void PipelineManager::Shutdown()
{
	if (m_device == VK_NULL_HANDLE) return;

	for (auto &val : m_pipelines | std::views::values)
	{
		Pipeline& s = val;
		if (s.pipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(m_device, s.pipeline, nullptr);
			s.pipeline = VK_NULL_HANDLE;
		}
	}

	// Destroy shader uniform buffers
	for (auto& buf : m_shaderDataBuffers) {
		if (buf.buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(VulkanCore::GetAllocator(), buf.buffer, buf.allocation);
			buf.buffer = VK_NULL_HANDLE;
			buf.allocation = VK_NULL_HANDLE;
		}
	}

	m_pipelines.clear();
}

void PipelineManager::CreatePipeline(const std::string& filePath, VkFormat depthFormat)
{
	Pipeline pipelineObject{ filePath };

	Slang::ComPtr<slang::ISession> slangSession;

	// THIS IS FUCKED >.>
	std::filesystem::path fullPath(filePath);
	std::filesystem::path dirPath = fullPath.parent_path();
	std::filesystem::path fileName = fullPath.stem();
	std::string dirPathStr = dirPath.string();
	const char* searchPathsArray[] = { dirPathStr.c_str() };
	m_slangSessionDesc.searchPaths = searchPathsArray;
	m_slangSessionDesc.searchPathCount = 1;

	m_slangGlobalSession->createSession(m_slangSessionDesc, slangSession.writeRef());

	if (slangSession == nullptr)
	{
		Logger::Error("Slang session is null!!");
	}
	
	// Check if file exists
	fs::path p = filePath; // or fs::path(fileName)
	if (!fs::exists(p) || !fs::is_regular_file(p)) {
		Logger::Info("Shder file not found from working directory: ", std::filesystem::current_path(), filePath);
		throw std::runtime_error("Shader file not found: " + fs::absolute(p).string());
	}

	Slang::ComPtr<ISlangBlob> diagnostics;
	Slang::ComPtr slangModule{ slangSession->loadModule(fileName.string().c_str(), diagnostics.writeRef())};

	if (slangModule == nullptr)
	{
		std::string msg;
		if (diagnostics && diagnostics->getBufferSize() > 0)
		{
			msg = std::string(static_cast<const char*>(diagnostics->getBufferPointer()), diagnostics->getBufferSize());		}
		else
		{
			msg = "No diagnostic output available.";
		}
		Logger::Error("Slang compilation failed for ", filePath, ":\n", msg);
		throw std::runtime_error("Slang compilation failed for " + filePath + ":\n" + msg);
	}

	Slang::ComPtr<ISlangBlob> spirv;
	Slang::Result spirvResult = slangModule->getTargetCode(0, spirv.writeRef());

	if (!SLANG_SUCCEEDED(spirvResult) || spirv == nullptr)
	{
		std::string msg;
		if (diagnostics && diagnostics->getBufferSize() > 0)
		{
			msg = std::string(static_cast<const char*>(diagnostics->getBufferPointer()), diagnostics->getBufferSize());
		}
		Logger::Error("SPIR-V generation failed for ", filePath, ":\n", msg);
		throw std::runtime_error("SPIR-V generation failed for " + filePath + ":\n" + msg);
	}

	// Create shader module from SPIR-V code
	VkShaderModuleCreateInfo shaderModuleCreateInfo{ };
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = spirv->getBufferSize();
	shaderModuleCreateInfo.pCode = static_cast<const uint32_t*>(spirv->getBufferPointer());
	VkShaderModule shaderModule{};
	if (vkCreateShaderModule(m_device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}
	
	//VkDescriptorBindingFlags descVariableFlag{ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };
	//VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .bindingCount = 1, .pBindingFlags = &descVariableFlag };
	//VkDescriptorSetLayoutBinding descLayoutBindingTex{ .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = static_cast<uint32_t>(1), .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT };
	//VkDescriptorSetLayoutCreateInfo descLayoutTexCI{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &descLayoutBindingTex };
	//if (vkCreateDescriptorSetLayout(m_device, &descLayoutTexCI, nullptr, &shader.descriptorSetLayoutTex) != VK_SUCCESS) {
	//	throw std::runtime_error("Failed to create descriptor set layout");
	//}

	constexpr VkFormat imageFormat{ VK_FORMAT_B8G8R8A8_SRGB };

	// Create layout for the graphics pipeline
	VkPushConstantRange pushConstantRange {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(PushConstants);
	
	// Create pipeline layout for the graphics pipeline
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	pipelineLayoutCreateInfo.pSetLayouts = GPUResourceUploader::GetDescriptorSetLayout();
	
	VkPipelineLayout pipelineLayout;
	if (vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pipeline layout");
	}
	
	// Defines the shader stages for the graphics pipeline. In this case, we are using a vertex and fragment shader.
	const char* name = "main";
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages{
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 
			.stage = VK_SHADER_STAGE_VERTEX_BIT, 
			.module = shaderModule, 
			.pName = name
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, 
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT, 
			.module = shaderModule, 
			.pName = name
		}
	};
	
	VkPipelineVertexInputStateCreateInfo vertexInputState {}; // No vertex buffer
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	// Defines how the vertices are assembled into primitives. In this case, we are using triangle lists.
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	std::vector<VkDynamicState> dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates.data();
	
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;
	
	VkPipelineRasterizationStateCreateInfo rasterizationState{};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleState{};
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencilState{};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilState.depthBoundsTestEnable = VK_FALSE;
	depthStencilState.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendState{};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &colorBlendAttachment;

	// Rendering info for dynamic rendering (no renderpass object)
	VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
	pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingInfo.colorAttachmentCount = 1;
	pipelineRenderingInfo.pColorAttachmentFormats = &imageFormat;
	pipelineRenderingInfo.depthAttachmentFormat = depthFormat;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.pNext = &pipelineRenderingInfo;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = VK_NULL_HANDLE;

	VkPipeline pipeline;
	if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create graphics pipeline");
	}

	pipelineObject.pipeline = pipeline;
	pipelineObject.pipelineLayout = pipelineLayout;
	m_pipelines[filePath] = pipelineObject;

	Logger::Success("Pipeline for ", filePath, " created successfully!");
}
