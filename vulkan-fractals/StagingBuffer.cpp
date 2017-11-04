//
// Created by jim on 11/3/17.
//

#include "StagingBuffer.h"

StagingBuffer::StagingBuffer(const VkPhysicalDevice &physicalDevice, const VkDevice &device)
        : mHostBuffer{physicalDevice, device}
          , mDeviceBuffer{physicalDevice, device}
{
}

void StagingBuffer::init(void *srcData, VkDeviceSize size, VkBufferUsageFlags usage, VkCommandPool pool, VkQueue queue)
{
    mHostBuffer.init(srcData, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    mDeviceBuffer.init(nullptr, size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    mHostBuffer.copyTo(mDeviceBuffer, pool, queue);
}

void StagingBuffer::destroy()
{
    mHostBuffer.destroy();
    mDeviceBuffer.destroy();
}

VkBuffer const & StagingBuffer::getBufferHandle()
{
    return mDeviceBuffer.getBufferHandle();
}
