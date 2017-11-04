#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "util.h"

class Buffer
{

private:

    const VkPhysicalDevice &mPhysicalDevice;
    const VkDevice &mDevice;
    VkBuffer mBuffer = nullptr;
    VkDeviceMemory mMemory = nullptr;
    VkDeviceSize mSize = 0;

public:

    Buffer(const VkPhysicalDevice &physicalDevice, const VkDevice &device);

    void destroy();

    void init(const void *srcData, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);

    void write(const void *srcData, VkDeviceSize offset = 0, VkDeviceSize size = 0);

    void copyTo(Buffer dst, VkCommandPool pool, VkQueue queue, VkDeviceSize size = 0);

    const VkBuffer &getBufferHandle() const;

    const VkDeviceMemory &getMemoryHandle() const;

    VkDeviceSize getSize() const;

private:

    /**
     * Find a suitable memory type for the specified filter and needed properties
     * @param filter Bitwise memory type filter
     * @param properties Needed memory properties
     * @return Index of suitable memory type
     */
    uint32_t findMemoryType(uint32_t filter, VkMemoryPropertyFlags properties);

};
