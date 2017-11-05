#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

struct QueueFamilyIndices
{
    uint32_t graphics;
    uint32_t present;
};

struct SwapchainParams
{
    VkPresentModeKHR presentMode;
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D extent;
};
