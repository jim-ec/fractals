#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <algorithm>
#include <chrono>
#include <thread>

#define GLFW_INCLUDE_VULKAN

#include "GLFW/glfw3.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

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
    static const int FPS;
    static const std::chrono::milliseconds RENDER_MILLIS;

    GLFWwindow *mWindow;
    std::chrono::system_clock::time_point mFPSSync;

    std::vector<const char *> mValidationLayers{{"VK_LAYER_LUNARG_standard_validation",
                                                        //"VK_LAYER_RENDERDOC_Capture"
                                                }};
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
    Buffer mUniformBuffer;
    VkCommandPool mCommandPool;
    std::vector<VkCommandBuffer> mCommandBuffers;
    VkSemaphore mImageAvailableSemaphore;
    VkSemaphore mRenderFinishedSemaphore;
    VkDescriptorPool mDescriptorPool;
    VkDescriptorSet mDescriptorSet;

    glm::vec2 mMoveDirections;
    int mZoomDirection = 0;
    float mCurrentZoom = 1;
    glm::vec2 mTranslation;

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

    struct UniformBufferObject
    {
        glm::mat4 model, view, proj;
        glm::vec4 fractalTransform;
    };

    void recreateSwapchain();

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

    void createBuffers();

    void createDescriptorPool();

    void createDescriptorSet();

    void createCommandPool();

    void createCommandBuffers();

    void createSemaphores();

    void draw();

    void updateUniformBuffer(const std::chrono::milliseconds &passedMillis);

    void syncWithFPS();

    /**
     * Vulkan debug report callback
     * @param flags
     * @param objType
     * @param obj
     * @param location
     * @param code
     * @param layerPrefix
     * @param msg
     * @param userData
     * @return
     */
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugReportFlagsEXT flags,
            VkDebugReportObjectTypeEXT objType,
            uint64_t obj,
            size_t location,
            int32_t code,
            const char *layerPrefix,
            const char *msg,
            void *userData);

    static void sOnKey(GLFWwindow *window, int key, int scancode, int action, int mods);

    void onKey(int key, int action);

    void destroySwapchain();

    static void onWindowResized(GLFWwindow *window, int width, int height);
};
