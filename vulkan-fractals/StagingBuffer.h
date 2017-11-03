#pragma once

#include "BareBuffer.h"

class StagingBuffer
{

public:

    StagingBuffer(const VkPhysicalDevice &physicalDevice, const VkDevice &device);

    void init(void *srcData, VkDeviceSize size, VkBufferUsageFlags usage, VkCommandPool pool, VkQueue queue);

    void destroy();

    VkBuffer getDeviceBufferHandle();

private:
    BareBuffer mHostBuffer, mDeviceBuffer;
};
