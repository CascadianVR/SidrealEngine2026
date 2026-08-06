#include "AccelerationStructure.h"

#include "Logger.h"
#include "Rendering/Loader.h"
#include "Rendering/RendererTypes.h"
#include "Rendering/Vulkan/GPUResourceUploader.h"
#include "Rendering/Vulkan/VulkanCore.h"

void AccelerationStructure::CreateBLASForMeshes()
{
    m_pfnCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(VulkanCore::GetDevice(), "vkCreateAccelerationStructureKHR"));
    m_pfnGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(VulkanCore::GetDevice(), "vkGetAccelerationStructureDeviceAddressKHR"));
    m_pfnGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(VulkanCore::GetDevice(), "vkGetAccelerationStructureBuildSizesKHR"));
    
    // For each mesh we generate the acceleration structure geometry
    const std::vector<Model>& models = Loader::GetLoadedModels();

    const VkDeviceAddress vertexAddress = GPUResourceUploader::GetVertexAddress();
    const VkDeviceAddress indexAddress = GPUResourceUploader::GetIndexAddress();

    uint32_t totalMeshCount = 0;
    for (const Model& model : models) for ([[maybe_unused]] const Mesh& mesh : model.meshes) totalMeshCount++;
    
    // Create a bottom level acceleration structure for each mesh
    m_geometries.reserve(totalMeshCount);
    m_primitiveCounts.reserve(totalMeshCount);
    m_blas.reserve(totalMeshCount);
    for (const Model& model : models)
    {
        for (const Mesh& mesh : model.meshes)
        {
            // Create geometry
            VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
            triangles.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            triangles.pNext                    = nullptr;
            triangles.vertexData.deviceAddress = vertexAddress + mesh.vertexOffset * sizeof(Vertex);
            triangles.indexData.deviceAddress  = indexAddress + mesh.indexOffset * sizeof(uint32_t);
            triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;
            triangles.vertexStride             = sizeof(Vertex);
            triangles.maxVertex                = mesh.vertexCount - 1;
            triangles.indexType                = VK_INDEX_TYPE_UINT32;
    
            VkAccelerationStructureGeometryKHR geometry{};
            geometry.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometry.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
            geometry.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geometry.geometry.triangles = triangles;
            
            m_geometries.push_back(geometry);
            
            uint32_t primitiveCount = mesh.indexCount / 3;
            m_primitiveCounts.push_back(primitiveCount);
            
            VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
            buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            buildInfo.geometryCount = 1;
            buildInfo.pGeometries   = &geometry;
            
            VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
            sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            
            m_pfnGetAccelerationStructureBuildSizesKHR(
                VulkanCore::GetDevice(),
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &buildInfo,
                &primitiveCount,
                &sizeInfo
            );
            
            // Create BLAS
            BLAS blas{};
            blas.size = sizeInfo.accelerationStructureSize;
            blas.scratchSize = sizeInfo.buildScratchSize;
            
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size  = blas.size;
            bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        
            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = 0;

            if (vmaCreateBuffer(VulkanCore::GetAllocator(), &bufferInfo, &allocInfo, &blas.buffer, &blas.allocation, nullptr) != VK_SUCCESS )
            {
                Logger::Error("Failed to create BLAS buffer");
            }
            
            VkAccelerationStructureCreateInfoKHR createInfo{};
            createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            createInfo.buffer = blas.buffer;
            createInfo.offset = 0;
            createInfo.size   = blas.size;
            createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

            if (m_pfnCreateAccelerationStructureKHR(VulkanCore::GetDevice(), &createInfo, nullptr, &blas.handle) != VK_SUCCESS)
            {
                Logger::Error("Failed to create BLAS");
            }
            
            VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
            addressInfo.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            addressInfo.accelerationStructure = blas.handle;

            blas.deviceAddress = m_pfnGetAccelerationStructureDeviceAddressKHR(VulkanCore::GetDevice(), &addressInfo);
            
            m_blas.push_back(blas);
            
            // Create instance data
            for (uint32_t i = 0; i < model.instanceCount; i++)
            {
                // Convert glm::mat4 to VkTransformMatrixKHR 
                VkTransformMatrixKHR vkMatrix;
                for (int row = 0; row < 3; ++row) {
                    for (int col = 0; col < 4; ++col) {
                        vkMatrix.matrix[row][col] = model.instanceMatrices[i][col][row];
                    }
                }
            
                VkAccelerationStructureInstanceKHR structureInstance{};
                structureInstance.transform = vkMatrix;
                //structureInstance.instanceCustomIndex = Not needed for now. I think;
                structureInstance.mask = 0xFF; // No masking that would exclude the instance
                structureInstance.instanceShaderBindingTableRecordOffset = 0;
                structureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR; // Hit both front and back
                structureInstance.accelerationStructureReference = blas.deviceAddress;
            
                m_instanceData.push_back(structureInstance);
            }
        }
    }
    
    // Upload instance data to GPU
    const VkDeviceSize bufferSize{ sizeof(VkAccelerationStructureInstanceKHR) * m_instanceData.size() };
    VkBufferCreateInfo bufferCreateInfo {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo bufferAllocationCreateInfo {};
    bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VmaAllocationInfo bufferAllocationInfo{};
    if (vmaCreateBuffer(VulkanCore::GetAllocator(), &bufferCreateInfo, &bufferAllocationCreateInfo, &m_instanceDataBuffer, &m_instanceDataBufferAllocation, &bufferAllocationInfo) != VK_SUCCESS)
    {
        Logger::Error("Failed to create instance data buffer");
        return;
    }

    if (bufferSize > 0) {
        memcpy(bufferAllocationInfo.pMappedData, m_instanceData.data(), bufferSize);
    }

    vmaFlushAllocation(VulkanCore::GetAllocator(), m_instanceDataBufferAllocation, 0, bufferSize);

    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = m_instanceDataBuffer;

    m_instanceDataBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &addressInfo);

    // Create combined BLAS scratch buffer
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{};
    accelProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &accelProps;
    vkGetPhysicalDeviceProperties2(VulkanCore::GetPhysicalDevice(), &props2);
    const VkDeviceSize scratchAlignment = accelProps.minAccelerationStructureScratchOffsetAlignment;
    VkDeviceSize totalScratchSize = 0;
    for (const BLAS& blas : m_blas)
    {
        totalScratchSize = (totalScratchSize + scratchAlignment - 1) & ~(scratchAlignment - 1);
        totalScratchSize += blas.scratchSize;
    }

    VkBufferCreateInfo scratchBufferInfo{};
    scratchBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    scratchBufferInfo.size = totalScratchSize;
    scratchBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo scratchAllocInfo{};
    scratchAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    scratchAllocInfo.flags = 0;

    if (vmaCreateBuffer(VulkanCore::GetAllocator(), &scratchBufferInfo, &scratchAllocInfo, &m_blasScratchBuffer, &m_blasScratchBufferAllocation, nullptr) != VK_SUCCESS)
    {
        Logger::Error("Failed to create BLAS scratch buffer");
    }

    VkBufferDeviceAddressInfo scratchAddressInfo{};
    scratchAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratchAddressInfo.buffer = m_blasScratchBuffer;

    m_blasScratchBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &scratchAddressInfo);

    Logger::Success("Created BLAS instance data buffer!");
}

void AccelerationStructure::CreateHLASForMeshes()
{
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.data.deviceAddress = m_instanceDataBufferDeviceAddress;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.flags = 0;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = instancesData;

    // Step 3: Build info
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    const uint32_t primitiveCount = static_cast<uint32_t>(m_instanceData.size());

    m_pfnGetAccelerationStructureBuildSizesKHR(
        VulkanCore::GetDevice(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo
    );

    // Create TLAS buffer
    m_tlas.size = sizeInfo.accelerationStructureSize;
    m_tlas.scratchSize = sizeInfo.buildScratchSize;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = m_tlas.size;
    bufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = 0;

    if (vmaCreateBuffer(VulkanCore::GetAllocator(), &bufferInfo, &allocInfo, &m_tlas.buffer, &m_tlas.allocation, nullptr) != VK_SUCCESS)
    {
        Logger::Error("Failed to create TLAS buffer");
    }

    // Create TLAS handle
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = m_tlas.buffer;
    createInfo.offset = 0;
    createInfo.size = m_tlas.size;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    if (m_pfnCreateAccelerationStructureKHR(VulkanCore::GetDevice(), &createInfo, nullptr, &m_tlas.handle) != VK_SUCCESS)
    {
        Logger::Error("Failed to create TLAS");
    }

    // Get TLAS device address
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addressInfo.accelerationStructure = m_tlas.handle;

    m_tlas.deviceAddress = m_pfnGetAccelerationStructureDeviceAddressKHR(VulkanCore::GetDevice(), &addressInfo);

    // Create TLAS scratch buffer
    VkBufferCreateInfo scratchBufferInfo{};
    scratchBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    scratchBufferInfo.size = m_tlas.scratchSize;
    scratchBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    VmaAllocationCreateInfo scratchAllocInfo{};
    scratchAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    scratchAllocInfo.flags = 0;

    if (vmaCreateBuffer(VulkanCore::GetAllocator(), &scratchBufferInfo, &scratchAllocInfo, &m_tlasScratchBuffer, &m_tlasScratchBufferAllocation, nullptr) != VK_SUCCESS)
    {
        Logger::Error("Failed to create TLAS scratch buffer");
    }

    VkBufferDeviceAddressInfo scratchAddressInfo{};
    scratchAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratchAddressInfo.buffer = m_tlasScratchBuffer;

    m_tlasScratchBufferDeviceAddress = vkGetBufferDeviceAddress(VulkanCore::GetDevice(), &scratchAddressInfo);

    Logger::Success("Created TLAS!");
}

void AccelerationStructure::BuildAccelerationStructures()
{
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    
    // Create temporary command buffer for uploading data
    VkCommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.pNext = nullptr;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCreateInfo.queueFamilyIndex = VulkanCore::GetQueueFamilyIndex();

    if (vkCreateCommandPool(VulkanCore::GetDevice(), &poolCreateInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool");
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(VulkanCore::GetDevice(), &allocInfo, &commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffer");
    }
    
    // Begin command buffer
    VkCommandBufferBeginInfo commandBufferBeginInfo{};
    commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    if (vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS)
    {
        Logger::Error("Failed to begin command buffer");
    }
    
    // Get pointer to necessary funtion
    m_pfnCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(VulkanCore::GetDevice(), "vkCmdBuildAccelerationStructuresKHR"));
    
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{};
    accelProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
    
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &accelProps;
    
    vkGetPhysicalDeviceProperties2(VulkanCore::GetPhysicalDevice(), &props2);
    
    const VkDeviceSize scratchAlignment = accelProps.minAccelerationStructureScratchOffsetAlignment;

    // Build BLAS
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> blasBuildInfos(m_blas.size());
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> blasBuildRangeInfos(m_blas.size());
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*> blasBuildRangeInfoPointers(m_blas.size());
    std::vector<VkDeviceAddress> blasScratchOffsets(m_blas.size());

    VkDeviceSize scratchOffset = 0;
    for (size_t i = 0; i < m_blas.size(); i++)
    {
        scratchOffset = (scratchOffset + scratchAlignment - 1) & ~(scratchAlignment - 1);
        blasScratchOffsets[i] = m_blasScratchBufferDeviceAddress + scratchOffset;
        scratchOffset += m_blas[i].scratchSize;

        blasBuildInfos[i].sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        blasBuildInfos[i].type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        blasBuildInfos[i].flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        blasBuildInfos[i].srcAccelerationStructure = VK_NULL_HANDLE;
        blasBuildInfos[i].dstAccelerationStructure = m_blas[i].handle;
        blasBuildInfos[i].geometryCount = 1;
        blasBuildInfos[i].pGeometries = &m_geometries[i];
        blasBuildInfos[i].mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        blasBuildInfos[i].scratchData.deviceAddress = blasScratchOffsets[i];

        blasBuildRangeInfos[i].primitiveCount = m_primitiveCounts[i];
        blasBuildRangeInfos[i].primitiveOffset = 0;

        blasBuildRangeInfoPointers[i] = &blasBuildRangeInfos[i];
    }

    m_pfnCmdBuildAccelerationStructuresKHR(commandBuffer, static_cast<uint32_t>(blasBuildInfos.size()), blasBuildInfos.data(), blasBuildRangeInfoPointers.data());

    // Barrier 1
    VkMemoryBarrier2 blasToTlasBarrier{};
    blasToTlasBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    blasToTlasBarrier.srcStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    blasToTlasBarrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    blasToTlasBarrier.dstStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    blasToTlasBarrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.memoryBarrierCount = 1;
    dependencyInfo.pMemoryBarriers = &blasToTlasBarrier;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
    
    // Build TLAS
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.data.deviceAddress = m_instanceDataBufferDeviceAddress;

    VkAccelerationStructureGeometryKHR tlasGeometry{};
    tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeometry.flags = 0;
    tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeometry.geometry.instances = instancesData;

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{};
    tlasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlasBuildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    tlasBuildInfo.dstAccelerationStructure = m_tlas.handle;
    tlasBuildInfo.geometryCount = 1;
    tlasBuildInfo.pGeometries = &tlasGeometry;
    tlasBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    tlasBuildInfo.scratchData.deviceAddress = m_tlasScratchBufferDeviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR tlasBuildRangeInfo{};
    tlasBuildRangeInfo.primitiveCount = static_cast<uint32_t>(m_instanceData.size());
    tlasBuildRangeInfo.primitiveOffset = 0;

    VkAccelerationStructureBuildRangeInfoKHR* tlasBuildRangeInfoPointer = &tlasBuildRangeInfo;

    m_pfnCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &tlasBuildInfo, &tlasBuildRangeInfoPointer);

    // Create pipeline memory barrier
    VkMemoryBarrier2 tlasToFragmentBarrier{};
    tlasToFragmentBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    tlasToFragmentBarrier.srcStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
    tlasToFragmentBarrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    tlasToFragmentBarrier.dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    tlasToFragmentBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    VkDependencyInfo dependencyInfo2{};
    dependencyInfo2.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo2.memoryBarrierCount       = 1;
    dependencyInfo2.pMemoryBarriers          = &tlasToFragmentBarrier;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo2);
    
    // End command buffer
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        Logger::Error("Failed to end command buffer");   
    }
    
    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    if (vkQueueSubmit(VulkanCore::GetQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
    {
        Logger::Error("Failed to submit command buffer");  
    }

    vkQueueWaitIdle(VulkanCore::GetQueue());

    // Destroy commandPool and commandBuffer
    vkFreeCommandBuffers(VulkanCore::GetDevice(), commandPool, 1, &commandBuffer);
    vkDestroyCommandPool(VulkanCore::GetDevice(), commandPool, nullptr);
    
    Logger::Success("Acceleration structures built!");
}

void AccelerationStructure::DestroyAccelerationStructures()
{
    VkDevice device = VulkanCore::GetDevice();
    VmaAllocator allocator = VulkanCore::GetAllocator();

    const auto pfnDestroyAccelerationStructureKHR =
        reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));

    // Destroy TLAS handle first (TLAS depends on BLAS)
    if (m_tlas.handle != VK_NULL_HANDLE)
    {
        pfnDestroyAccelerationStructureKHR(device, m_tlas.handle, nullptr);
        m_tlas.handle = VK_NULL_HANDLE;
    }

    // Destroy BLAS handles
    for (BLAS& blas : m_blas)
    {
        if (blas.handle != VK_NULL_HANDLE)
        {
            pfnDestroyAccelerationStructureKHR(device, blas.handle, nullptr);
            blas.handle = VK_NULL_HANDLE;
        }
    }

    // Destroy TLAS buffer before BLAS buffers
    if (m_tlas.buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(allocator, m_tlas.buffer, m_tlas.allocation);
        m_tlas.buffer = VK_NULL_HANDLE;
        m_tlas.allocation = VK_NULL_HANDLE;
    }

    // Destroy BLAS buffers
    for (BLAS& blas : m_blas)
    {
        if (blas.buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(allocator, blas.buffer, blas.allocation);
            blas.buffer = VK_NULL_HANDLE;
            blas.allocation = VK_NULL_HANDLE;
        }
    }
    
    // Destroy scratch and instance buffers
    if (m_blasScratchBuffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(allocator, m_blasScratchBuffer, m_blasScratchBufferAllocation);
        m_blasScratchBuffer = VK_NULL_HANDLE;
        m_blasScratchBufferAllocation = VK_NULL_HANDLE;
    }
    if (m_tlasScratchBuffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(allocator, m_tlasScratchBuffer, m_tlasScratchBufferAllocation);
        m_tlasScratchBuffer = VK_NULL_HANDLE;
        m_tlasScratchBufferAllocation = VK_NULL_HANDLE;
    }
    if (m_instanceDataBuffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(allocator, m_instanceDataBuffer, m_instanceDataBufferAllocation);
        m_instanceDataBuffer = VK_NULL_HANDLE;
        m_instanceDataBufferAllocation = VK_NULL_HANDLE;
    }
}