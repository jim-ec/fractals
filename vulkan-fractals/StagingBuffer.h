#pragma once

#include "Buffer.h"

class StagingBuffer
{

public:

    StagingBuffer(const VkPhysicalDevice &physicalDevice, const VkDevice &device);

    void init(const void *srcData, VkDeviceSize size, VkBufferUsageFlags usage, VkCommandPool pool, VkQueue queue);

    void destroy();

    VkBuffer const &getBufferHandle();

private:
    Buffer mHostBuffer, mDeviceBuffer;
};
