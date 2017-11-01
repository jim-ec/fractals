#include "Application.h"

int Application::sInstanceCount = 0;

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

        uint32_t count;
        vkEnumerateInstanceLayerProperties(&count, nullptr);
        std::vector<VkLayerProperties> availableLayers{count};
        vkEnumerateInstanceLayerProperties(&count, availableLayers.data());

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
        for (size_t i = 0; i < requestedAreAvailable.size(); i++) {
            check(requestedAreAvailable[i], "Requested layer ", mValidationLayers[i], " is not available");
        }
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

    // Create instance
    {

        VkInstanceCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.enabledExtensionCount = mExtensions.size();
        info.ppEnabledExtensionNames = mExtensions.data();
        info.enabledLayerCount = mValidationLayers.size();
        info.ppEnabledLayerNames = mValidationLayers.data();

        checkVk(vkCreateInstance(&info, nullptr, &mInstance), "Cannot create Vulkan instance");
    }

    // Create debug report:
    {
        VkDebugReportCallbackCreateInfoEXT info = {};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        info.pfnCallback = &debugCallback;
        info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;

        checkVk(invokeVk<PFN_vkCreateDebugReportCallbackEXT>(mInstance, "vkCreateDebugReportCallbackEXT", &info,
                nullptr, &mDebugCallback),
                "Cannot create debug report callback");
    }

    // Get physical device:
    {
        uint32_t count;
        vkEnumeratePhysicalDevices(mInstance, &count, nullptr);
        std::vector<VkPhysicalDevice> physicalDevices{count};
        vkEnumeratePhysicalDevices(mInstance, &count, physicalDevices.data());
    }
}

Application::~Application()
{
    invokeVk<PFN_vkDestroyDebugReportCallbackEXT>(mInstance, "vkDestroyDebugReportCallbackEXT", mDebugCallback,
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

VkBool32 Application::debugCallback(
        VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t,
        size_t, int32_t, const char *, const char *msg, void *)
{
    std::cout << "VK-DEBUG: " << msg << std::endl;
    return VK_FALSE;
}
