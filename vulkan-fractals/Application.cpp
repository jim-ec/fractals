#include "Application.h"

const int Application::FPS = 40;
const std::chrono::milliseconds Application::RENDER_MILLIS{1000 / FPS};

int Application::sInstanceCount = 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"

Application::Application()
        : mVertexBuffer{mPhysicalDevice, mDevice}
          , mIndexBuffer{mPhysicalDevice, mDevice}
          , mPipeline{mPhysicalDevice, mDevice}
          , mUniformBuffer{mPhysicalDevice, mDevice}
{
    // Create window:
    if (0 == sInstanceCount++) {
        glfwInit();
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    mWindow = glfwCreateWindow(800, 600, "Fractal Viewer", nullptr, nullptr);
    glfwSetWindowUserPointer(mWindow, this);
    glfwSetKeyCallback(mWindow, &sOnKey);
    glfwSetWindowSizeCallback(mWindow, &onWindowResized);

    fmt::printf("VULKAN - FRACTAL VIEWER:\n"
                        "============================================================\n"
                        "WASD - Arrow keys  -  Move around\n"
                        "QE - Page Up/Down  -  Zoom in/out faster/slower\n"
                        "F                  -  Toggle fullscreen\n"
                        "Esc                -  Quit\n"
                        "Space              -  Stop zoom / Reset to starting position\n"
                        "============================================================\n");

#ifndef NDEBUG
    // Validation layers:
    if (!mValidationLayers.empty()) {
        log("Load validation layers ...");

        std::vector<bool> requestedAreAvailable;
        requestedAreAvailable.resize(mValidationLayers.size());
        auto availableLayers = listVulkan<VkLayerProperties>(&vkEnumerateInstanceLayerProperties);

        // Check for available layers:
        for (auto &available : availableLayers) {
            for (size_t i = 0; i < mValidationLayers.size(); i++) {
                if (!strcmp(available.layerName, mValidationLayers[i])) {
                    requestedAreAvailable[i] = true;
                    log("Enable validation layer: %s", available.layerName);
                    break;
                }
            }
        }

        // Print unsupported layers:
        auto iter = std::find(requestedAreAvailable.begin(), requestedAreAvailable.end(), false);
        if (iter != requestedAreAvailable.end()) {
            check(false, "Requested layer ", mValidationLayers[std::distance(requestedAreAvailable.begin(), iter)],
                    " is not available");
        }
    }
#endif // NDEBUG

    // Extensions:
    {
        uint32_t glfwExtensionCount;
        const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        for (uint32_t i = 0; i < glfwExtensionCount; i++) {
            mInstanceExtensions.push_back(glfwExtensions[i]);
        }

#ifndef NDEBUG
        mInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif // NDEBUG
    }

    createInstance();
    setupDebugReport();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createSwapchainViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createBuffers();
    createDescriptorPool();
    createDescriptorSet();
    createCommandBuffers();
    createSemaphores();

    fmt::print("Window creation completed\n");
}

void Application::onWindowResized(GLFWwindow *window, int width, int height)
{
    if (width == 0 || height == 0) { return; }
    reinterpret_cast<Application *>(glfwGetWindowUserPointer(window))->recreateSwapchain();
}

void Application::recreateSwapchain()
{
    vkDeviceWaitIdle(mDevice);

    destroySwapchain();

    vkDeviceWaitIdle(mDevice);

    createSwapchain();
    createSwapchainViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createDescriptorSet();
    createCommandBuffers();
}

void Application::destroySwapchain()
{
    checkVk(vkFreeDescriptorSets(mDevice, mDescriptorPool, 1, &mDescriptorSet), "Cannot free descriptor set");
    for (auto &framebuffer : mSwapchainFramebuffers) {
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }
    vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
    mPipeline.destroy();
    for (auto &view : mSwapchainImageViews) {
        vkDestroyImageView(mDevice, view, nullptr);
    }
    vkFreeCommandBuffers(mDevice, mCommandPool, static_cast<uint32_t>(mCommandBuffers.size()), mCommandBuffers.data());
    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
}

void Application::toggleFullscreen()
{
    if (nullptr == glfwGetWindowMonitor(mWindow)) {
        // Enable fullscreen:
        int w, h;
        glfwGetWindowPos(mWindow, &mWindowedWindowPos.x, &mWindowedWindowPos.y);
        glfwGetWindowSize(mWindow, &mWindowedWindowSize.x, &mWindowedWindowSize.y);
        glfwGetMonitorPhysicalSize(glfwGetPrimaryMonitor(), &w, &h);
        glfwSetWindowMonitor(mWindow, glfwGetPrimaryMonitor(), 0, 0, w, h, GLFW_DONT_CARE);
    }
    else {
        // Disable fullscreen:
        glfwSetWindowMonitor(mWindow, nullptr, mWindowedWindowPos.x, mWindowedWindowPos.y, mWindowedWindowSize.x,
                             mWindowedWindowSize.y, GLFW_DONT_CARE);
    }
}

void Application::createInstance()
{
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.enabledExtensionCount = static_cast<uint32_t>(mInstanceExtensions.size());
    info.ppEnabledExtensionNames = mInstanceExtensions.data();
    info.enabledLayerCount = static_cast<uint32_t>(mValidationLayers.size());
    info.ppEnabledLayerNames = mValidationLayers.data();

    checkVk(vkCreateInstance(&info, nullptr, &mInstance), "Cannot create Vulkan instance");
}

void Application::setupDebugReport()
{
#ifndef NDEBUG
    VkDebugReportCallbackCreateInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    info.pfnCallback = &debugCallback;
    info.flags = DEBUG_REPORT_FLAGS;

    checkVk(invokeVk(vkCreateDebugReportCallbackEXT, mInstance, &info, nullptr, &mDebugCallback),
            "Cannot create debug report callback");
#endif // NDEBUG
}

void Application::createSurface()
{
    glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface);
}

void Application::pickPhysicalDevice()
{
    auto physicalDevices = listVulkan<VkPhysicalDevice>(&vkEnumeratePhysicalDevices, mInstance);
    check(!physicalDevices.empty(), "No physical devices available");

    auto deviceIter = std::find_if(physicalDevices.begin(), physicalDevices.end(), [this](auto &device) {

        // Check for required extensions:
        auto availableExtensions = listVulkan<VkExtensionProperties>(&vkEnumerateDeviceExtensionProperties, device,
                                                                     nullptr);
        if (std::find_if(mDeviceExtensions.begin(), mDeviceExtensions.end(),
                         [&availableExtensions](const char *requiredExtension) {
                             return std::find_if(availableExtensions.begin(), availableExtensions.end(),
                                                 [&requiredExtension](const VkExtensionProperties &extension) {
                                                     return !strcmp(extension.extensionName, requiredExtension);
                                                 }) != availableExtensions.end();
                         }) == mDeviceExtensions.end()) {
            return false;
        }

        // Check for present modes and find best one:
        auto presentModes = listVulkan<VkPresentModeKHR>(&vkGetPhysicalDeviceSurfacePresentModesKHR, device, mSurface);
        if (presentModes.empty()) {
            return false;
        }
        if (std::find_if(presentModes.begin(), presentModes.end(), [this](const auto &mode) {
            if (VK_PRESENT_MODE_MAILBOX_KHR == mode) {
                mSwapchainParams.presentMode = mode;
                return true;
            }
            return false;
        }) == presentModes.end()) {
            mSwapchainParams.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        }

        // Check for formats and find best one:
        auto formats = listVulkan<VkSurfaceFormatKHR>(&vkGetPhysicalDeviceSurfaceFormatsKHR, device, mSurface);
        if (formats.empty()) {
            return false;
        }
        if (formats.size() == 1 && VK_FORMAT_UNDEFINED == formats[0].format) {
            mSwapchainParams.surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
            mSwapchainParams.surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        }
        else {
            if (std::find_if(formats.begin(), formats.end(), [this](const auto &format) {
                if (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
                    format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    mSwapchainParams.surfaceFormat = format;
                    return true;
                }
                return false;
            }) == formats.end()) {
                mSwapchainParams.surfaceFormat = formats[0];
            }
        }

        return true;
    });
    check(deviceIter != physicalDevices.end(), "Cannot find a physical device with an adequate swapchain");

    mPhysicalDevice = *deviceIter;

    {
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(mPhysicalDevice, &properties);
        const char *type;
        switch (properties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                type = "CPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                type = "GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                type = "integrated GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                type = "virtual GPU";
                break;
            default:
                type = "other";
                break;
        }
        fmt::printf("Physical device:\n"
                            "    Name: %s\n"
                            "    Type: %s\n"
                            "    Vulkan API: %d.%d.%d\n", properties.deviceName, type,
                    VK_VERSION_MAJOR(properties.apiVersion), VK_VERSION_MINOR(properties.apiVersion),
                    VK_VERSION_PATCH(properties.apiVersion));
    }
}

void Application::createLogicalDevice()
{
    // Get queue family index:
    {
        auto queueFamilies = listVulkan<VkQueueFamilyProperties>(&vkGetPhysicalDeviceQueueFamilyProperties,
                                                                 mPhysicalDevice);

        // Find graphics queue:
        auto iter = std::find_if(queueFamilies.begin(), queueFamilies.end(), [](auto &family) {
            return family.queueFlags & VK_QUEUE_GRAPHICS_BIT;
        });
        check(iter != queueFamilies.end(), "Cannot find a graphics queue family");
        mQueueFamilyIndices.graphics = static_cast<uint32_t>(std::distance(queueFamilies.begin(), iter));

        // Find present queue:
        iter = std::find_if(queueFamilies.begin(), queueFamilies.end(), [this, i = 0u](auto &family) mutable {
            VkBool32 supported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, i, mSurface, &supported);
            return VK_TRUE == supported;
        });
        check(iter != queueFamilies.end(), "Cannot find a present queue family");
        mQueueFamilyIndices.present = static_cast<uint32_t>(std::distance(queueFamilies.begin(), iter));
    }

    float queuePriorities[] = {1.0};

    std::set<uint32_t> uniqueQueueFamilyIndices{mQueueFamilyIndices.graphics, mQueueFamilyIndices.present};
    std::vector<VkDeviceQueueCreateInfo> queueInfos;

    for (auto &uniqueQueueFamilyIndex : uniqueQueueFamilyIndices) {
        VkDeviceQueueCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueCount = 1;
        info.pQueuePriorities = queuePriorities;
        info.queueFamilyIndex = uniqueQueueFamilyIndex;
        queueInfos.push_back(info);
    }

    VkDeviceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    info.pQueueCreateInfos = queueInfos.data();
    info.enabledExtensionCount = static_cast<uint32_t>(mDeviceExtensions.size());
    info.ppEnabledExtensionNames = mDeviceExtensions.data();
    info.pEnabledFeatures = &mDeviceFeatures;

    checkVk(vkCreateDevice(mPhysicalDevice, &info, nullptr, &mDevice), "Cannot create logical device");

    // Get queues:
    vkGetDeviceQueue(mDevice, mQueueFamilyIndices.graphics, 0, &mGraphicsQueue);
    if (uniqueQueueFamilyIndices.size() == 1) {
        // Queues are the same
        mPresentQueue = mGraphicsQueue;
    }
    else {
        vkGetDeviceQueue(mDevice, mQueueFamilyIndices.present, 0, &mPresentQueue);
    }
}

void Application::createSwapchain()
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
        int width, height;
        glfwGetWindowSize(mWindow, &width, &height);
        mSwapchainParams.extent.width = clamp(static_cast<uint32_t>(width), caps.minImageExtent.width,
                                              caps.maxImageExtent.width);
        mSwapchainParams.extent.height = clamp(static_cast<uint32_t>(height), caps.minImageExtent.height,
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

void Application::createSwapchainViews()
{
    mSwapchainImageViews.resize(mSwapchainImages.size());

    log("Create swapchain image views: %d", mSwapchainImages.size());

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

        checkVk(vkCreateImageView(mDevice, &info, nullptr, &mSwapchainImageViews[i]), "Cannot create an image view");
    }
}

void Application::createRenderPass()
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

void Application::createGraphicsPipeline()
{
    mPipeline.init(mSwapchainParams.extent, mRenderPass);
}

void Application::createFramebuffers()
{
    mSwapchainFramebuffers.resize(mSwapchainImages.size());

    for (size_t i = 0; i < mSwapchainFramebuffers.size(); i++) {
        std::array<VkImageView, 1> attachments{{mSwapchainImageViews[i]}};

        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.attachmentCount = attachments.size();
        info.pAttachments = attachments.data();
        info.renderPass = mRenderPass;
        info.width = mSwapchainParams.extent.width;
        info.height = mSwapchainParams.extent.height;
        info.layers = 1;

        checkVk(vkCreateFramebuffer(mDevice, &info, nullptr, &mSwapchainFramebuffers[i]), "Cannot create framebuffer");
    }
}

void Application::createBuffers()
{
    mVertexBuffer.init(mVertices.data(), byteSize(mVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mCommandPool,
                       mGraphicsQueue);
    mIndexBuffer.init(mIndices.data(), byteSize(mIndices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mCommandPool,
                      mGraphicsQueue);
    mUniformBuffer.init(nullptr, sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void Application::createCommandPool()
{
    VkCommandPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = mQueueFamilyIndices.graphics;

    checkVk(vkCreateCommandPool(mDevice, &info, nullptr, &mCommandPool), "Cannot create command pool");
}

void Application::createDescriptorPool()
{
    VkDescriptorPoolSize size = {};
    size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    size.descriptorCount = 1;

    VkDescriptorPoolCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    info.maxSets = 1;
    info.poolSizeCount = 1;
    info.pPoolSizes = &size;

    checkVk(vkCreateDescriptorPool(mDevice, &info, nullptr, &mDescriptorPool), "Cannot create descriptor pool");
}

void Application::createDescriptorSet()
{
    {
        VkDescriptorSetAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool = mDescriptorPool;
        info.descriptorSetCount = 1;
        info.pSetLayouts = &mPipeline.getDescriptorSetLayout();

        checkVk(vkAllocateDescriptorSets(mDevice, &info, &mDescriptorSet), "Cannot allocate descriptor set");
    }
    {
        VkDescriptorBufferInfo info = {};
        info.buffer = mUniformBuffer.getBufferHandle();
        info.offset = 0;
        info.range = mUniformBuffer.getSize();

        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &info;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.dstSet = mDescriptorSet;

        vkUpdateDescriptorSets(mDevice, 1, &write, 0, nullptr);
    }
}

void Application::createCommandBuffers()
{
    // Allocate command buffers:
    mCommandBuffers.resize(mSwapchainFramebuffers.size());
    {
        VkCommandBufferAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());
        info.commandPool = mCommandPool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        checkVk(vkAllocateCommandBuffers(mDevice, &info, mCommandBuffers.data()), "Cannot allocate command buffers");
    }

    // Record command buffers:
    for (size_t i = 0; i < mCommandBuffers.size(); i++) {
        auto &buffer = mCommandBuffers[i];

        // Begin command buffer:
        VkCommandBufferBeginInfo begin = {};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        vkBeginCommandBuffer(buffer, &begin);

        // Begin render pass:
        VkClearValue clearColor = {1.0f, 1.0f, 1.0f, 1.0f};

        VkRenderPassBeginInfo beginRenderPass = {};
        beginRenderPass.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginRenderPass.renderPass = mRenderPass;
        beginRenderPass.framebuffer = mSwapchainFramebuffers[i];
        beginRenderPass.renderArea.offset = {0, 0};
        beginRenderPass.renderArea.extent = mSwapchainParams.extent;
        beginRenderPass.clearValueCount = 1;
        beginRenderPass.pClearValues = &clearColor;

        vkCmdBeginRenderPass(buffer, &beginRenderPass, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline.getHandle());

        std::array<VkDeviceSize, 1> offsets = {0};
        vkCmdBindVertexBuffers(buffer, 0, 1, &mVertexBuffer.getBufferHandle(), offsets.data());

        vkCmdBindIndexBuffer(buffer, mIndexBuffer.getBufferHandle(), 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline.getLayout(), 0, 1, &mDescriptorSet,
                                0, nullptr);

        vkCmdDrawIndexed(buffer, static_cast<uint32_t>(mIndices.size()), 1, 0, 0, 0);
        vkCmdEndRenderPass(buffer);
        checkVk(vkEndCommandBuffer(buffer), "Cannot record command buffer");
    }
}

void Application::createSemaphores()
{
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    checkVk(vkCreateSemaphore(mDevice, &info, nullptr, &mImageAvailableSemaphore), "Cannot create semaphore");
    checkVk(vkCreateSemaphore(mDevice, &info, nullptr, &mRenderFinishedSemaphore), "Cannot create semaphore");
}

Application::~Application()
{
    destroySwapchain();
    vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
    mUniformBuffer.destroy();
    mVertexBuffer.destroy();
    mIndexBuffer.destroy();
    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
    vkDestroySemaphore(mDevice, mImageAvailableSemaphore, nullptr);
    vkDestroySemaphore(mDevice, mRenderFinishedSemaphore, nullptr);
    vkDestroyDevice(mDevice, nullptr);
    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
#ifndef NDEBUG
    invokeVk(vkDestroyDebugReportCallbackEXT, mInstance, mDebugCallback, nullptr);
#endif // NDEBUG
    vkDestroyInstance(mInstance, nullptr);
    glfwDestroyWindow(mWindow);
    if (0 == --sInstanceCount) {
        glfwTerminate();
    }
}

void Application::syncWithFPS()
{
    auto now = std::chrono::system_clock::now();
    mFPSSync += RENDER_MILLIS;
    auto waitMillis = std::chrono::duration_cast<std::chrono::milliseconds>(mFPSSync - now);

    if (waitMillis.count() < 0) {
        mFPSSync = now;
    }
    else {
        std::this_thread::sleep_for(waitMillis);
    }
}

void Application::updateUniformBuffer(const std::chrono::milliseconds &passedMillis)
{
    UniformBufferObject ubo = {};
    float aspect = static_cast<float>(mSwapchainParams.extent.width) / mSwapchainParams.extent.height;

    mCurrentZoom *= 1 + mZoomDirection * 0.01;

    mTranslation.x += 0.06 * mMoveDirections.x / mCurrentZoom;
    mTranslation.y += 0.06 * mMoveDirections.y / mCurrentZoom;

    ubo.model = glm::scale(glm::mat4{}, glm::vec3{mCurrentZoom});
    ubo.view = glm::translate(ubo.view, glm::vec3{0, 0, -1});
    ubo.proj = glm::ortho(-1.f, 1.f, 1.f, -1.f, 0.1f, 10.f);

    ubo.fractalTransform.x = aspect * 2.0f / mCurrentZoom;
    ubo.fractalTransform.y = 1 * 2.0f / mCurrentZoom;
    ubo.fractalTransform[2] = mTranslation.x;
    ubo.fractalTransform[3] = mTranslation.y;

    mUniformBuffer.write(&ubo);
}

void Application::run()
{
    mFPSSync = std::chrono::system_clock::now();

    auto last = std::chrono::system_clock::now();

    while (!glfwWindowShouldClose(mWindow)) {
        glfwPollEvents();

        auto now = std::chrono::system_clock::now();
        updateUniformBuffer(std::chrono::duration_cast<std::chrono::milliseconds>(now - last));
        last = now;

        draw();

        // Wait if rendering was too fast:
        syncWithFPS();
    }

    vkDeviceWaitIdle(mDevice);
}

void Application::draw()
{
    vkQueueWaitIdle(mPresentQueue);

    uint32_t imageIndex;
    {
        auto result = vkAcquireNextImageKHR(mDevice, mSwapchain, std::numeric_limits<uint64_t>::max(),
                                            mImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            check(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "Cannot acquire next swapchain image");
        }
    }

    VkCommandBuffer *commandBuffer = &mCommandBuffers[imageIndex];

    {
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.commandBufferCount = 1;
        info.pCommandBuffers = commandBuffer;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &mImageAvailableSemaphore;
        info.pWaitDstStageMask = waitStages;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &mRenderFinishedSemaphore;

        checkVk(vkQueueSubmit(mGraphicsQueue, 1, &info, VK_NULL_HANDLE), "Cannot submit queue");
    }

    {
        VkPresentInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &mRenderFinishedSemaphore;
        info.swapchainCount = 1;
        info.pSwapchains = &mSwapchain;
        info.pImageIndices = &imageIndex;

        checkVk(vkQueuePresentKHR(mPresentQueue, &info), "Cannot present on swapchain");
    }
}

#pragma clang diagnostic pop // ignored "-Wreturn-stack-address"

VkBool32 Application::debugCallback(VkDebugReportFlagsEXT flags,
        VkDebugReportObjectTypeEXT,
        uint64_t,
        size_t,
        int32_t,
        const char *,
        const char *msg,
        void *)
{
    bool error = false;
    const char *type;

    switch (flags) {
        case VK_DEBUG_REPORT_ERROR_BIT_EXT:
            error = true;
            type = "ERROR";
            break;

        case VK_DEBUG_REPORT_WARNING_BIT_EXT:
            type = "WARN";
            break;

        case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:
            type = "INFO";
            break;

        case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
            type = "PERFORMANCE";
            break;

        case VK_DEBUG_REPORT_DEBUG_BIT_EXT:
            type = "DEBUG";
            break;

        default:
            type = "UNKNOWN ERROR TYPE";
    }

    fmt::printf("VK-LOG[%s]: %s\n", type, msg);
    if (error) {
        throw std::runtime_error{msg};
    }
    return VK_FALSE;
}

void Application::sOnKey(GLFWwindow *window, int key, int, int action, int)
{
    reinterpret_cast<Application *>(glfwGetWindowUserPointer(window))->onKey(key, action);
}

void Application::onKey(int key, int action)
{
    bool pressed = action != GLFW_RELEASE;

    switch (key) {
        case GLFW_KEY_UP:
        case GLFW_KEY_W:
            mMoveDirections.y = pressed ? -1 : 0;
            break;

        case GLFW_KEY_DOWN:
        case GLFW_KEY_S:
            mMoveDirections.y = pressed ? 1 : 0;
            break;

        case GLFW_KEY_LEFT:
        case GLFW_KEY_A:
            mMoveDirections.x = pressed ? -1 : 0;
            break;

        case GLFW_KEY_RIGHT:
        case GLFW_KEY_D:
            mMoveDirections.x = pressed ? 1 : 0;
            break;

        case GLFW_KEY_PAGE_UP:
        case GLFW_KEY_Q:
            if (pressed) {
                mZoomDirection++;
            }
            break;

        case GLFW_KEY_PAGE_DOWN:
        case GLFW_KEY_E:
            if (pressed) {
                mZoomDirection--;
            }
            break;

        case GLFW_KEY_SPACE:
            if (!pressed) {
                break;
            }
            if (mZoomDirection != 0) {
                // Currently zooming => stop zoom
                mZoomDirection = 0;
            }
            else {
                // No zoom => return to default view
                mCurrentZoom = 1;
                mTranslation = {};
            }
            break;

        case GLFW_KEY_F:
            if (!pressed) {
                return;
            }
            toggleFullscreen();
            break;

        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(mWindow, GLFW_TRUE);
            break;

        default:
            break;
    }
}
