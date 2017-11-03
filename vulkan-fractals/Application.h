#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#define GLFW_INCLUDE_VULKAN

#include "GLFW/glfw3.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "glm/glm.hpp"

#include "Vertex.h"
#include "util.h"
#include "StagingBuffer.h"
#include "Pipeline.h"

#pragma once

class Application
{

public:

    Application();

    ~Application();

    void run();

private:
    static int sInstanceCount;
    static const uint32_t WINDOW_WIDTH;
    static const uint32_t WINDOW_HEIGHT;

    GLFWwindow *mWindow;
    std::vector<const char *> mValidationLayers{{"VK_LAYER_LUNARG_standard_validation", /*"VK_LAYER_RENDERDOC_Capture"*/ }};
    std::vector<const char *> mInstanceExtensions{{VK_EXT_DEBUG_REPORT_EXTENSION_NAME}};
    std::vector<const char *> mDeviceExtensions{{VK_KHR_SWAPCHAIN_EXTENSION_NAME}};
    VkDebugReportCallbackEXT mDebugCallback;
    VkSurfaceKHR mSurface;
    VkPhysicalDevice mPhysicalDevice;
    VkInstance mInstance;
    VkDevice mDevice;
    VkQueue mGraphicsQueue;
    VkQueue mPresentQueue;
    VkSwapchainKHR mSwapchain;
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;
    VkRenderPass mRenderPass;
    Pipeline mPipeline;
    std::vector<VkFramebuffer> mSwapchainFramebuffers;
    StagingBuffer mVertexBuffer;
    StagingBuffer mIndexBuffer;
    VkCommandPool mCommandPool;
    std::vector<VkCommandBuffer> mCommandBuffers;
    VkSemaphore mImageAvailableSemaphore;
    VkSemaphore mRenderFinishedSemaphore;

    std::vector<Vertex> mVertices{{{-1, -1}, {1, -1}, {-1, 1}, {1, 1}}};
    std::vector<uint16_t> mIndices{{0, 1, 2, 2, 1, 3}};

    struct
    {
        uint32_t graphics = static_cast<uint32_t>(-1);
        uint32_t present = static_cast<uint32_t>(-1);
    } mQueueFamilyIndices;

    struct
    {
        VkPresentModeKHR presentMode;
        VkSurfaceFormatKHR surfaceFormat;
        VkExtent2D extent;
    } mSwapchainParams;

    void createInstance();

    void setupDebugReport();

    void createSurface();

    void pickPhysicalDevice();

    void createLogicalDevice();

    void createSwapchain();

    void createSwapchainViews();

    void createRenderPass();

    void createGraphicsPipeline();

    void createFramebuffers();

    void createVertexBuffer();

    void createCommandBuffers();

    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugReportFlagsEXT flags,
            VkDebugReportObjectTypeEXT objType,
            uint64_t obj,
            size_t location,
            int32_t code,
            const char *layerPrefix,
            const char *msg,
            void *userData);

    void createCommandPool();
};
