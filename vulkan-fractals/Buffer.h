#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "util.h"

class Buffer
{

private:

    const VkPhysicalDevice &mPhysicalDevice;
    const VkDevice &mDevice;
    VkBuffer mBuffer;
    VkDeviceMemory mMemory;

public:

    Buffer(const VkPhysicalDevice &physicalDevice, const VkDevice &device);

    ~Buffer();

    void init(VkMemoryPropertyFlags properties, VkDeviceSize size, void *srcData);

    const VkBuffer &getBufferHandle() const;

    const VkDeviceMemory &getMemoryHandle() const;

private:

    /**
     * Find a suitable memory type for the specified filter and needed properties
     * @param filter Bitwise memory type filter
     * @param properties Needed memory properties
     * @return Index of suitable memory type
     */
    uint32_t findMemoryType(uint32_t filter, VkMemoryPropertyFlags properties);

};
