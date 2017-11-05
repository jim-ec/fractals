#pragma once

#include <vulkan/vulkan.h>

#include "util.h"
#include "Pipeline.h"
#include "QueueFamilyIndices.h"

class Swapchain
{

public:

    Swapchain(const VkPhysicalDevice &physicalDevice,
            const VkDevice &device,
            const VkSurfaceKHR &surface,
            const QueueFamilyIndices &queueFamilyIndices,
            SwapchainParams &swapchainParams);

    void init(int windowWidth, int windowHeight);

    uint32_t acquireNextImage(VkSemaphore signalSemaphore);

    void destroy();

    const Pipeline &getPipeline();

    const VkRenderPass &getRenderPassHandle() const;

    const std::vector<VkFramebuffer> &getFramebuffers() const;

    int getSwapchainImageCount() const;

    const VkSwapchainKHR &getSwapchainHandle() const;

private:

    const VkPhysicalDevice &mPhysicalDevice;
    const VkDevice &mDevice;
    const VkSurfaceKHR &mSurface;
    const QueueFamilyIndices &mQueueFamilyIndices;
    SwapchainParams &mSwapchainParams;

    VkSwapchainKHR mSwapchain;
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;
    Pipeline mPipeline;
    std::vector<VkFramebuffer> mFramebuffers;
    VkRenderPass mRenderPass;

};
