#pragma once

#include <vulkan/vulkan.h>

struct SwapchainParams
{
    VkPresentModeKHR presentMode;
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D extent;
};
