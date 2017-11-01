#include <iostream>
#include <vector>

#define GLFW_INCLUDE_VULKAN

#include "GLFW/glfw3.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "glm/glm.hpp"

#include <iostream>
#include <string>
#include "util.h"

#pragma once

class Application {

public:

    Application();

    ~Application();

    void run();

private:
    static int sInstanceCount;
    GLFWwindow *mWindow;
    std::vector<const char *> mValidationLayers{{"VK_LAYER_LUNARG_standard_validation", "VK_LAYER_RENDERDOC_Capture"}};
    std::vector<const char *> mExtensions{{VK_EXT_DEBUG_REPORT_EXTENSION_NAME}};
    VkInstance mInstance;
    VkPhysicalDevice mPhysicalDevice;
    VkDevice mDevice;
    VkDebugReportCallbackEXT mDebugCallback;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugReportFlagsEXT flags,
            VkDebugReportObjectTypeEXT objType,
            uint64_t obj,
            size_t location,
            int32_t code,
            const char *layerPrefix,
            const char *msg,
            void *userData);

};


