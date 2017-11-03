#include "Buffer.h"

Buffer::Buffer(const VkPhysicalDevice &physicalDevice, const VkDevice &device)
        : mDevice{device}
          , mPhysicalDevice{physicalDevice}
{
}

void Buffer::init(VkMemoryPropertyFlags properties, VkDeviceSize size, void *srcData)
{
    {
        VkBufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        info.size = size;

        checkVk(vkCreateBuffer(mDevice, &info, nullptr, &mBuffer), "Cannot create buffer");
    }
    {
        VkMemoryRequirements req = {};
        vkGetBufferMemoryRequirements(mDevice, mBuffer, &req);

        auto memoryTypeIndex = findMemoryType(req.memoryTypeBits, properties);

        VkMemoryAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = req.size;
        info.memoryTypeIndex = memoryTypeIndex;

        checkVk(vkAllocateMemory(mDevice, &info, nullptr, &mMemory), "Cannot allocate vertex buffer memory");

        vkBindBufferMemory(mDevice, mBuffer, mMemory, 0);
    }
    {
        void *data;
        vkMapMemory(mDevice, mMemory, 0, size, 0, &data);
        memcpy(data, srcData, size);
        vkUnmapMemory(mDevice, mMemory);
    }
}

Buffer::~Buffer()
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
