#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "util.h"

class Buffer
{

private:

    const VkPhysicalDevice &mPhysicalDevice;
    const VkDevice &mDevice;
    VkBuffer mBuffer = 0;
    VkDeviceMemory mMemory = 0;
    VkDeviceSize mSize = 0;

public:

    Buffer(const VkPhysicalDevice &physicalDevice, const VkDevice &device);

    void destroy();

    void init(VkMemoryPropertyFlags properties, VkBufferUsageFlags usage, VkDeviceSize size, void *srcData);

    void copyTo(Buffer dst, VkCommandPool pool, VkQueue queue, VkDeviceSize size = 0);

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
