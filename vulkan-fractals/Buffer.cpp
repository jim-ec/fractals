#include "Buffer.h"

Buffer::Buffer(const VkPhysicalDevice &physicalDevice, const VkDevice &device)
        : mDevice{device}
          , mPhysicalDevice{physicalDevice}
{
}

void Buffer::init(const void *srcData, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
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
        void *mappedMemory;
        vkMapMemory(mDevice, mMemory, 0, size, 0, &mappedMemory);
        memcpy(mappedMemory, srcData, size);
        vkUnmapMemory(mDevice, mMemory);
    }
}

void Buffer::destroy()
{
    vkFreeMemory(mDevice, mMemory, nullptr);
    vkDestroyBuffer(mDevice, mBuffer, nullptr);
}

uint32_t Buffer::findMemoryType(uint32_t filter, VkMemoryPropertyFlags properties)
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

const VkBuffer &Buffer::getBufferHandle() const
{
    return mBuffer;
}

const VkDeviceMemory &Buffer::getMemoryHandle() const
{
    return mMemory;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"

void Buffer::copyTo(Buffer dst, VkCommandPool pool, VkQueue queue, VkDeviceSize size)
{
    if (0 == size) {
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

void Buffer::write(const void *srcData, VkDeviceSize offset, VkDeviceSize size)
{
    if (0 == size) {
        size = mSize;
    }

    void *mappedMemory;
    vkMapMemory(mDevice, mMemory, offset, size, 0, &mappedMemory);
    memcpy(mappedMemory, srcData, static_cast<size_t>(size));
    vkUnmapMemory(mDevice, mMemory);
}

VkDeviceSize Buffer::getSize() const
{
    return mSize;
}

#pragma clang diagnostic pop
