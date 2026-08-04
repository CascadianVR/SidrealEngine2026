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
    
    m_geometries.reserve(totalMeshCount);
    m_primitiveCounts.reserve(totalMeshCount);
    m_blas.reserve(totalMeshCount);
    for (const Model& model : models)
    {
        for (const Mesh& mesh : model.meshes)
        {
            // Create triangles
            VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
            triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            triangles.vertexData.deviceAddress = vertexAddress + mesh.vertexOffset * sizeof(Vertex);
            triangles.vertexStride = sizeof(Vertex);
            triangles.maxVertex = mesh.vertexCount - 1;
            triangles.indexType = VK_INDEX_TYPE_UINT32;
            triangles.indexData.deviceAddress = indexAddress + mesh.indexOffset * sizeof(uint32_t);
    
            VkAccelerationStructureGeometryKHR geometry{};
            geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geometry.geometry.triangles = triangles;
            geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
            
            m_geometries.push_back(geometry);
            
            uint32_t primitiveCount = mesh.indexCount / 3;
            m_primitiveCounts.push_back(primitiveCount);
            
            VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
            buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
            buildInfo.type =VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            buildInfo.flags =VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
            buildInfo.geometryCount = 1;
            buildInfo.pGeometries = &geometry;
            
            VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
            sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
            
            m_pfnGetAccelerationStructureBuildSizesKHR(
                VulkanCore::GetDevice(),
                VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                &buildInfo,
                &primitiveCount,
                &sizeInfo
            );
            
            BLAS blas{};
            blas.size = sizeInfo.accelerationStructureSize;
            
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = blas.size;
            bufferInfo.usage =VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        
            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = 0;

            if (vmaCreateBuffer(VulkanCore::GetAllocator(), &bufferInfo, &allocInfo, &blas.buffer, &blas.allocation, nullptr) != VK_SUCCESS )
            {
                Logger::Error("Failed to create BLAS buffer");
            }
            
            VkAccelerationStructureCreateInfoKHR createInfo{};
            createInfo.sType =VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
            createInfo.buffer = blas.buffer;
            createInfo.offset = 0;
            createInfo.size = blas.size;
            createInfo.type =VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

            if (m_pfnCreateAccelerationStructureKHR(VulkanCore::GetDevice(), &createInfo, nullptr, &blas.handle) != VK_SUCCESS)
            {
                Logger::Error("Failed to create BLAS");
            }
            
            VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
            addressInfo.sType =
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;

            addressInfo.accelerationStructure = blas.handle;

            blas.deviceAddress = m_pfnGetAccelerationStructureDeviceAddressKHR(VulkanCore::GetDevice(), &addressInfo);
            
            m_blas.push_back(blas);
        }
    }
}
