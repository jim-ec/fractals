//
// Created by jim on 11/5/17.
//

#include "Swapchain.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"

Swapchain::Swapchain(const VkPhysicalDevice &physicalDevice,
        const VkDevice &device,
        const VkSurfaceKHR &surface,
        const QueueFamilyIndices &queueFamilyIndices,
        SwapchainParams &swapchainParams)
        : mPipeline{physicalDevice, device}
          , mPhysicalDevice{physicalDevice}
          , mDevice{device}
          , mSurface{surface}
          , mQueueFamilyIndices(queueFamilyIndices)
          , mSwapchainParams(swapchainParams)
{
}

void Swapchain::init(int windowWidth, int windowHeight)
{
    // Create actual swapchain:
    {
        VkSurfaceCapabilitiesKHR caps = {};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &caps);

        VkSwapchainCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.imageFormat = mSwapchainParams.surfaceFormat.format;
        info.imageColorSpace = mSwapchainParams.surfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = mSwapchainParams.presentMode;
        info.surface = mSurface;
        info.clipped = VK_TRUE;
        info.oldSwapchain = VK_NULL_HANDLE;

        // Find best swap extent:
        if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max() ||
            caps.currentExtent.height != std::numeric_limits<uint32_t>::max()) {
            mSwapchainParams.extent = caps.currentExtent;
        }
        else {
            mSwapchainParams.extent.width = clamp(static_cast<uint32_t>(windowWidth), caps.minImageExtent.width,
                    caps.maxImageExtent.width);
            mSwapchainParams.extent.height = clamp(static_cast<uint32_t>(windowHeight), caps.minImageExtent.height,
                    caps.maxImageExtent.height);
        }
        info.imageExtent = mSwapchainParams.extent;

        // Set image count:
        if (caps.maxImageCount == 0 || caps.maxImageCount > caps.minImageCount + 1) {
            info.minImageCount = caps.minImageCount + 1;
        }
        else {
            info.minImageCount = caps.minImageCount;
        }

        if (mQueueFamilyIndices.graphics == mQueueFamilyIndices.present) {
            info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        else {
            info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            std::array<uint32_t, 2> indices{{mQueueFamilyIndices.graphics, mQueueFamilyIndices.present}};
            info.queueFamilyIndexCount = indices.size();
            info.pQueueFamilyIndices = indices.data();
        }

        checkVk(vkCreateSwapchainKHR(mDevice, &info, nullptr, &mSwapchain), "Cannot create swapchain");

        // Get swapchain images:
        mSwapchainImages = listVulkan<VkImage>(&vkGetSwapchainImagesKHR, mDevice, mSwapchain);
    }

    // Create swapchain views:
    {
        mSwapchainImageViews.resize(mSwapchainImages.size());

        for (size_t i = 0; i < mSwapchainImages.size(); i++) {
            VkImageViewCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.format = mSwapchainParams.surfaceFormat.format;
            info.image = mSwapchainImages[i];
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.baseArrayLayer = 0;
            info.subresourceRange.layerCount = 1;
            info.subresourceRange.baseMipLevel = 0;
            info.subresourceRange.levelCount = 1;

            checkVk(vkCreateImageView(mDevice, &info, nullptr, &mSwapchainImageViews[i]),
                    "Cannot create an image view");
        }
    }

    // Create render pass
    {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = mSwapchainParams.surfaceFormat.format;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference attachmentRef = {};
        attachmentRef.attachment = 0;
        attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &attachmentRef;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkSubpassDependency dep = {};
        dep.srcSubpass = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass = 0;
        dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = 0;
        dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &colorAttachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dep;

        checkVk(vkCreateRenderPass(mDevice, &info, nullptr, &mRenderPass), "Cannot create render pass");
    }

    // Create graphics pipeline:
    mPipeline.init(mSwapchainParams.extent, mRenderPass);

    // Create framebuffers:
    {
        mFramebuffers.resize(mSwapchainImages.size());

        for (size_t i = 0; i < mFramebuffers.size(); i++) {
            std::array<VkImageView, 1> attachments{{mSwapchainImageViews[i]}};

            VkFramebufferCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.attachmentCount = attachments.size();
            info.pAttachments = attachments.data();
            info.renderPass = mRenderPass;
            info.width = mSwapchainParams.extent.width;
            info.height = mSwapchainParams.extent.height;
            info.layers = 1;

            checkVk(vkCreateFramebuffer(mDevice, &info, nullptr, &mFramebuffers[i]), "Cannot create framebuffer");
        }
    }
}

void Swapchain::destroy()
{
    for (auto &framebuffer : mFramebuffers) {
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }
    vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
    mPipeline.destroy();
    for (auto &view : mSwapchainImageViews) {
        vkDestroyImageView(mDevice, view, nullptr);
    }
    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
}

int Swapchain::getSwapchainImageCount() const
{
    return static_cast<int>(mSwapchainImages.size());
}

const Pipeline &Swapchain::getPipeline()
{
    return mPipeline;
}

const VkRenderPass &Swapchain::getRenderPassHandle() const
{
    return mRenderPass;
}

const std::vector<VkFramebuffer> &Swapchain::getFramebuffers() const
{
    return mFramebuffers;
}

uint32_t Swapchain::acquireNextImage(VkSemaphore signalSemaphore)
{
    uint32_t imageIndex;
    auto result = vkAcquireNextImageKHR(mDevice, mSwapchain, std::numeric_limits<uint64_t>::max(), signalSemaphore,
            VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        destroy();
        init(mSwapchainParams.extent.width, mSwapchainParams.extent.height);
        check(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "Cannot acquire next swapchain image");
    }
    return imageIndex;
}

const VkSwapchainKHR &Swapchain::getSwapchainHandle() const
{
    return mSwapchain;
}

#pragma clang diagnostic pop
