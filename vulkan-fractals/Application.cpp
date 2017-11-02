#include "Application.h"

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
    mWindow = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);

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
            mExtensions.push_back(glfwExtensions[i]);
        }

        mExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }

    createInstance();
    setupDebugReport();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    fmt::print("Window creation completed\n");
}

void Application::createInstance()
{
    VkInstanceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.enabledExtensionCount = static_cast<uint32_t>(mExtensions.size());
    info.ppEnabledExtensionNames = mExtensions.data();
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

    checkVk(invokeVk<PFN_vkCreateDebugReportCallbackEXT>("vkCreateDebugReportCallbackEXT", mInstance, &info, nullptr,
                                                         &mDebugCallback), "Cannot create debug report callback");
}

void Application::createSurface()
{
    glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface);
}

void Application::pickPhysicalDevice()
{
    auto physicalDevices = listVulkan<VkPhysicalDevice>(&vkEnumeratePhysicalDevices, mInstance);

    check(!physicalDevices.empty(), "No physical devices available");

    auto deviceIter = std::find_if(physicalDevices.begin(), physicalDevices.end(), [](auto &device) {
        auto extensions = listVulkan<VkExtensionProperties>(&vkEnumerateDeviceExtensionProperties, device, nullptr);
        return std::find_if(extensions.begin(), extensions.end(), [](const VkExtensionProperties &extension) {
            return !strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }) != extensions.end();
    });
    check(deviceIter != physicalDevices.end(), "Cannot find a physical device with swapchain support: ",
          VK_KHR_SWAPCHAIN_EXTENSION_NAME);

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

#pragma clang diagnostic pop // ignored "-Wreturn-stack-address"

Application::~Application()
{
    vkDestroyDevice(mDevice, nullptr);
    vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
    invokeVk<PFN_vkDestroyDebugReportCallbackEXT>("vkDestroyDebugReportCallbackEXT", mInstance, mDebugCallback,
                                                  nullptr);
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
