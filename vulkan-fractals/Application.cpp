#include "Application.h"

const int Application::FPS = 40;
const std::chrono::milliseconds Application::RENDER_MILLIS{1000 / FPS};

int Application::sInstanceCount = 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"

Application::Application()
        : mVertexBuffer{mPhysicalDevice, mDevice}
          , mIndexBuffer{mPhysicalDevice, mDevice}
          , mUniformBuffer{mPhysicalDevice, mDevice}
          , mSwapchain{mPhysicalDevice, mDevice, mSurface, mQueueFamilyIndices, mSwapchainParams}
{
    // Create window:
    if (0 == sInstanceCount++) {
        glfwInit();
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    mWindow = glfwCreateWindow(1200, 900, "Fractal Viewer", nullptr, nullptr);
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
            "x                  -  Print current zoom\n"
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

    {
        int width, height;
        glfwGetWindowSize(mWindow, &width, &height);
        mSwapchain.init(width, height);
    }

    createCommandPool();
    createBuffers();
    createDescriptorPool();
    createDescriptorSet();
    createCommandBuffers();
    createSemaphores();
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

    {
        int width, height;
        glfwGetWindowSize(mWindow, &width, &height);
        mSwapchain.init(width, height);
    }

    createDescriptorSet();
    createCommandBuffers();
}

void Application::destroySwapchain()
{
    checkVk(vkFreeDescriptorSets(mDevice, mDescriptorPool, 1, &mDescriptorSet), "Cannot free descriptor set");
    mSwapchain.destroy();
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
                        "    Vulkan API: %d.%d.%d\n", properties.deviceName, type, VK_VERSION_MAJOR(properties.apiVersion),
                VK_VERSION_MINOR(properties.apiVersion), VK_VERSION_PATCH(properties.apiVersion));
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
    {
        VkCommandPoolCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        info.queueFamilyIndex = mQueueFamilyIndices.graphics;

        checkVk(vkCreateCommandPool(mDevice, &info, nullptr, &mCommandPool), "Cannot create command pool");
    }

    // Allocate command buffers:
    mCommandBuffers.resize(static_cast<unsigned long>(mSwapchain.getSwapchainImageCount()));
    {
        VkCommandBufferAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());
        info.commandPool = mCommandPool;
        info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

        checkVk(vkAllocateCommandBuffers(mDevice, &info, mCommandBuffers.data()), "Cannot allocate command buffers");
    }
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
        info.pSetLayouts = &mSwapchain.getPipeline().getDescriptorSetLayout();

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
        beginRenderPass.renderPass = mSwapchain.getRenderPassHandle();
        beginRenderPass.framebuffer = mSwapchain.getFramebuffers()[i];
        beginRenderPass.renderArea.offset = {0, 0};
        beginRenderPass.renderArea.extent = mSwapchainParams.extent;
        beginRenderPass.clearValueCount = 1;
        beginRenderPass.pClearValues = &clearColor;

        vkCmdBeginRenderPass(buffer, &beginRenderPass, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mSwapchain.getPipeline().getHandle());

        std::array<VkDeviceSize, 1> offsets = {0};
        vkCmdBindVertexBuffers(buffer, 0, 1, &mVertexBuffer.getBufferHandle(), offsets.data());

        vkCmdBindIndexBuffer(buffer, mIndexBuffer.getBufferHandle(), 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mSwapchain.getPipeline().getLayout(), 0, 1,
                &mDescriptorSet,
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

    /*uint32_t imageIndex;
    {
        auto result = vkAcquireNextImageKHR(mDevice, mSwapchain, std::numeric_limits<uint64_t>::max(),
                mImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            check(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "Cannot acquire next swapchain image");
        }
    }*/
    auto imageIndex = mSwapchain.acquireNextImage(mImageAvailableSemaphore);

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
        info.pSwapchains = &mSwapchain.getSwapchainHandle();
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

        case GLFW_KEY_X:
            if (!pressed) {
                return;
            }
            fmt::printf("Current zoom: %dx\n", static_cast<int>(mCurrentZoom));
            break;

        default:
            break;
    }
}
