#pragma once
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

class AccelerationStructure
{
public:
    struct BLAS
    {
        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkDeviceAddress deviceAddress = 0;
        VkDeviceSize size = 0;
        VkDeviceSize scratchSize = 0;
    };

    struct TLAS
    {
        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkDeviceAddress deviceAddress = 0;
        VkDeviceSize size = 0;
        VkDeviceSize scratchSize = 0;
    };
    
    static void CreateBLASForMeshes();
    static void CreateHLASForMeshes();
    static void BuildAccelerationStructures();
    static void DestroyAccelerationStructures();

    static VkAccelerationStructureKHR* GetTLAS() { return &m_tlas.handle; }
    
private:
    static inline std::vector<VkAccelerationStructureInstanceKHR> m_instanceData;
    static inline VkBuffer m_instanceDataBuffer = VK_NULL_HANDLE;
    static inline VmaAllocation m_instanceDataBufferAllocation = VK_NULL_HANDLE;
    static inline VkDeviceAddress m_instanceDataBufferDeviceAddress;
    
    static inline std::vector<VkAccelerationStructureGeometryKHR> m_geometries;
    static inline std::vector<uint32_t> m_primitiveCounts;
    static inline std::vector<BLAS> m_blas;
    static inline TLAS m_tlas;
    
    static inline VkBuffer m_blasScratchBuffer = VK_NULL_HANDLE;
    static inline VmaAllocation m_blasScratchBufferAllocation = VK_NULL_HANDLE;
    static inline VkDeviceAddress m_blasScratchBufferDeviceAddress = 0;
    static inline VkBuffer m_tlasScratchBuffer = VK_NULL_HANDLE;
    static inline VmaAllocation m_tlasScratchBufferAllocation = VK_NULL_HANDLE;
    static inline VkDeviceAddress m_tlasScratchBufferDeviceAddress = 0;
    
    static inline PFN_vkCreateAccelerationStructureKHR m_pfnCreateAccelerationStructureKHR;
    static inline PFN_vkGetAccelerationStructureDeviceAddressKHR m_pfnGetAccelerationStructureDeviceAddressKHR;
    static inline PFN_vkGetAccelerationStructureBuildSizesKHR m_pfnGetAccelerationStructureBuildSizesKHR;
    static inline PFN_vkCmdBuildAccelerationStructuresKHR m_pfnCmdBuildAccelerationStructuresKHR;
};
