#pragma once
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

class AccelerationStructure
{
public:
    static void CreateBLASForMeshes();
    
    struct BLAS
    {
        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;

        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;

        VkDeviceAddress deviceAddress = 0;

        VkDeviceSize size = 0;
    };
    
private:
    static inline std::vector<VkAccelerationStructureGeometryKHR> m_geometries;
    static inline std::vector<uint32_t> m_primitiveCounts;
    static inline std::vector<BLAS> m_blas;
    
    static inline PFN_vkCreateAccelerationStructureKHR m_pfnCreateAccelerationStructureKHR;
    static inline PFN_vkGetAccelerationStructureDeviceAddressKHR m_pfnGetAccelerationStructureDeviceAddressKHR;
    static inline PFN_vkGetAccelerationStructureBuildSizesKHR m_pfnGetAccelerationStructureBuildSizesKHR;
};
