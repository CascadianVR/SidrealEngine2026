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
    
    // Get
    uint32_t totalMeshCount = 0;
    for (const Model& model : models) for (const Mesh& mesh : model.meshes) totalMeshCount++;
    
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
            triangles.vertexData.deviceAddress = vertexAddress + mesh.vertexOffset * sizeof(Vertex);
            triangles.indexData.deviceAddress  = indexAddress + mesh.indexOffset * sizeof(uint32_t);
            triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;
            triangles.maxVertex                = mesh.vertexCount - 1;
            triangles.vertexStride             = sizeof(Vertex);
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

    Logger::Success("Created TLAS!");
}
