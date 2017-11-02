#include "Application.h"

const uint32_t Application::WINDOW_WIDTH = 800;
const uint32_t Application::WINDOW_HEIGHT = 600;

int Application::sInstanceCount = 0;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"

Application::Application()
{
    // Create window:
    if (0 == sInstanceCount++) {
        glfwInit();
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    mWindow = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan window", nullptr, nullptr);

    // Validation layers:
    {
        std::vector<bool> requestedAreAvailable;
        requestedAreAvailable.resize(mValidationLayers.size());
        auto availableLayers = listVulkan<VkLayerProperties>(&vkEnumerateInstanceLayerProperties);

        // Check for available layers:
        for (auto &available : availableLayers) {
            for (size_t i = 0; i < mValidationLayers.size(); i++) {
                if (!strcmp(available.layerName, mValidationLayers[i])) {
                    requestedAreAvailable[i] = true;
                    mValidationLayers.push_back(mValidationLayers[i]);
                    break;
                }
            }
        }

        // Print unsupported layers:
        auto iter = std::find(requestedAreAvailable.begin(), requestedAreAvailable.end(), false);
        check(iter == requestedAreAvailable.end(), "Requested layer ",
              mValidationLayers[std::distance(requestedAreAvailable.begin(), iter)], " is not available");
    }

    // Extensions:
    {
        uint32_t glfwExtensionCount;
        const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        for (uint32_t i = 0; i < glfwExtensionCount; i++) {
            mInstanceExtensions.push_back(glfwExtensions[i]);
        }

        mInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    createInstance();
    setupDebugReport();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain();
    createSwapchainViews();
    createGraphicsPipeline();

    fmt::print("Window creation completed\n");
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
    VkDebugReportCallbackCreateInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    info.pfnCallback = &debugCallback;
    info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;

    checkVk(invokeVk(vkCreateDebugReportCallbackEXT, mInstance, &info, nullptr, &mDebugCallback),
            "Cannot create debug report callback");
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
        mSwapchainParams.extent.width = clamp(WINDOW_WIDTH, caps.minImageExtent.width, caps.maxImageExtent.width);
        mSwapchainParams.extent.height = clamp(WINDOW_HEIGHT, caps.minImageExtent.height, caps.maxImageExtent.height);
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

void Application::createGraphicsPipeline()
{
    // Create vertex shader:
    {
        auto vertShaderCode = readRawFile("shaders/quad.vert.spv");

        VkShaderModuleCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = vertShaderCode.size();
        info.pCode = reinterpret_cast<const uint32_t *>(vertShaderCode.data());

        checkVk(vkCreateShaderModule(mDevice, &info, nullptr, &mShaderModules[0]), "Cannot create vertex shader");
    }

    // Create fragment shader:
    {
        auto fragShaderCode = readRawFile("shaders/quad.frag.spv");

        VkShaderModuleCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = fragShaderCode.size();
        info.pCode = reinterpret_cast<const uint32_t *>(fragShaderCode.data());

        checkVk(vkCreateShaderModule(mDevice, &info, nullptr, &mShaderModules[1]), "Cannot create fragment shader");
    }
}

#pragma clang diagnostic pop // ignored "-Wreturn-stack-address"

Application::~Application()
{
    for (auto &module : mShaderModules) {
        vkDestroyShaderModule(mDevice, module, nullptr);
    }
    for (auto &view : mSwapchainImageViews) {
        vkDestroyImageView(mDevice, view, nullptr);
    }
    vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
    vkDestroyDevice(mDevice, nullptr);
    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    invokeVk(vkDestroyDebugReportCallbackEXT, mInstance, mDebugCallback, nullptr);
    vkDestroyInstance(mInstance, nullptr);
    glfwDestroyWindow(mWindow);
    if (0 == --sInstanceCount) {
        glfwTerminate();
    }
}

void Application::run()
{
    while (!glfwWindowShouldClose(mWindow)) {
        glfwPollEvents();
    }
}

VkBool32
Application::debugCallback(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char *,
        const char *msg, void *)
{
    std::cout << "VK-DEBUG: " << msg << std::endl;
    return VK_FALSE;
}
