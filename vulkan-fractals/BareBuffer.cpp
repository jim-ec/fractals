#include "BareBuffer.h"

BareBuffer::BareBuffer(const VkPhysicalDevice &physicalDevice, const VkDevice &device)
        : mDevice{device}
          , mPhysicalDevice{physicalDevice}
{
}

void BareBuffer::init(VkMemoryPropertyFlags properties, VkBufferUsageFlags usage, VkDeviceSize size, void *srcData)
{
    mSize = size;

    // Create buffer:
    {
        VkBufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.usage = usage;
        info.size = size;

        checkVk(vkCreateBuffer(mDevice, &info, nullptr, &mBuffer), "Cannot create buffer");
    }

    // Allocate underlying memory:
    {
        VkMemoryRequirements req = {};
        vkGetBufferMemoryRequirements(mDevice, mBuffer, &req);

        auto memoryTypeIndex = findMemoryType(req.memoryTypeBits, properties);

        VkMemoryAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = req.size;
        info.memoryTypeIndex = memoryTypeIndex;

        checkVk(vkAllocateMemory(mDevice, &info, nullptr, &mMemory), "Cannot allocate buffer memory");

        vkBindBufferMemory(mDevice, mBuffer, mMemory, 0);
    }

    // Optionally map and write memory:
    if (nullptr != srcData) {
        void *data;
        vkMapMemory(mDevice, mMemory, 0, size, 0, &data);
        memcpy(data, srcData, size);
        vkUnmapMemory(mDevice, mMemory);
    }
}

void BareBuffer::destroy()
{
    vkFreeMemory(mDevice, mMemory, nullptr);
    vkDestroyBuffer(mDevice, mBuffer, nullptr);
}

uint32_t BareBuffer::findMemoryType(uint32_t filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps = {};
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if (filter & (1 << i) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    check(false, "Cannot find suitable memory type");
}

const VkBuffer &BareBuffer::getBufferHandle() const
{
    return mBuffer;
}

const VkDeviceMemory &BareBuffer::getMemoryHandle() const
{
    return mMemory;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"

void BareBuffer::copyTo(BareBuffer dst, VkCommandPool pool, VkQueue queue, VkDeviceSize size)
{
    if (size == 0) {
        size = mSize;
    }

    VkCommandBuffer commandBuffer;

    // Build command buffer:

    VkCommandBufferAllocateInfo allocation = {};
    allocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocation.commandBufferCount = 1;
    allocation.commandPool = pool;
    allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    checkVk(vkAllocateCommandBuffers(mDevice, &allocation, &commandBuffer),
            "Cannot create command buffer to copy buffer");

    VkCommandBufferBeginInfo begin = {};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &begin);

    VkBufferCopy copy = {};
    copy.size = size;
    vkCmdCopyBuffer(commandBuffer, mBuffer, dst.mBuffer, 1, &copy);

    checkVk(vkEndCommandBuffer(commandBuffer), "Cannot create buffer copy command buffer");

    // Submit command buffer:

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffer;

    checkVk(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "Cannot submit buffer copy");

    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(mDevice, pool, 1, &commandBuffer);
}

#pragma clang diagnostic pop
