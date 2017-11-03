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
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createVertexBuffer();
    setupCommands();

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
        auto fragShaderCode = readRawFile("shaders/mandelbrot.frag.spv");

        VkShaderModuleCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = fragShaderCode.size();
        info.pCode = reinterpret_cast<const uint32_t *>(fragShaderCode.data());

        checkVk(vkCreateShaderModule(mDevice, &info, nullptr, &mShaderModules[1]), "Cannot create fragment shader");
    }

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageInfos = {};

    shaderStageInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfos[0].module = mShaderModules[0];
    shaderStageInfos[0].pName = "main";
    shaderStageInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;

    shaderStageInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfos[1].module = mShaderModules[1];
    shaderStageInfos[1].pName = "main";
    shaderStageInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;

    auto binding = Vertex::getBindingDescription();
    auto attributes = Vertex::getAttributeDescription();

    VkPipelineVertexInputStateCreateInfo vertexInputState = {};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount = 1;
    vertexInputState.pVertexBindingDescriptions = &binding;
    vertexInputState.vertexAttributeDescriptionCount = attributes.size();
    vertexInputState.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.primitiveRestartEnable = VK_FALSE;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {};
    viewport.height = mSwapchainParams.extent.height;
    viewport.width = mSwapchainParams.extent.width;
    viewport.x = 0;
    viewport.y = 0;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;

    VkRect2D scissor = {};
    scissor.extent = mSwapchainParams.extent;
    scissor.offset = {0, 0};

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;

    VkPipelineRasterizationStateCreateInfo rasterizationState = {};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationState.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleState = {};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.alphaToCoverageEnable = VK_FALSE;
    multisampleState.alphaToOneEnable = VK_FALSE;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.sampleShadingEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = &colorBlendAttachment;
    colorBlendState.logicOpEnable = VK_FALSE;

    {
        VkPipelineLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.pushConstantRangeCount = 0;
        info.setLayoutCount = 0;

        checkVk(vkCreatePipelineLayout(mDevice, &info, nullptr, &mPipelineLayout), "Cannot create pipeline layout");
    }

    VkGraphicsPipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.layout = mPipelineLayout;
    info.stageCount = shaderStageInfos.size();
    info.pStages = shaderStageInfos.data();
    info.pColorBlendState = &colorBlendState;
    info.pInputAssemblyState = &inputAssemblyState;
    info.pMultisampleState = &multisampleState;
    info.pRasterizationState = &rasterizationState;
    info.pVertexInputState = &vertexInputState;
    info.renderPass = mRenderPass;
    info.subpass = 0;
    info.pViewportState = &viewportState;

    checkVk(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &info, nullptr, &mPipeline),
            "Cannot create graphics pipeline");
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


void Application::createVertexBuffer()
{
    {
        std::array<uint32_t, 1> families = {mQueueFamilyIndices.graphics};

        VkBufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        info.size = mVertices.size() * sizeof(Vertex);

        checkVk(vkCreateBuffer(mDevice, &info, nullptr, &mVertexBuffer), "Cannot create vertex buffer");
    }
    {
        VkMemoryRequirements req = {};
        vkGetBufferMemoryRequirements(mDevice, mVertexBuffer, &req);

        auto memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkMemoryAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = req.size;
        info.memoryTypeIndex = memoryTypeIndex;

        checkVk(vkAllocateMemory(mDevice, &info, nullptr, &mVertexBufferMemory),
                "Cannot allocate vertex buffer memory");

        vkBindBufferMemory(mDevice, mVertexBuffer, mVertexBufferMemory, 0);
    }
    {
        VkDeviceSize size = mVertices.size() * sizeof(Vertex);

        void *data;
        vkMapMemory(mDevice, mVertexBufferMemory, 0, size, 0, &data);
        memcpy(data, mVertices.data(), size);
        vkUnmapMemory(mDevice, mVertexBufferMemory);
    }
}

void Application::setupCommands()
{
    // Create command pool:
    {
        VkCommandPoolCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.queueFamilyIndex = mQueueFamilyIndices.graphics;

        checkVk(vkCreateCommandPool(mDevice, &info, nullptr, &mCommandPool), "Cannot create command pool");
    }

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
        {
            VkCommandBufferBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

            vkBeginCommandBuffer(buffer, &info);
        }

        // Begin render pass:
        {
            VkClearValue clearColor = {1.0f, 1.0f, 1.0f, 1.0f};

            VkRenderPassBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            info.renderPass = mRenderPass;
            info.framebuffer = mSwapchainFramebuffers[i];
            info.renderArea.offset = {0, 0};
            info.renderArea.extent = mSwapchainParams.extent;
            info.clearValueCount = 1;
            info.pClearValues = &clearColor;

            vkCmdBeginRenderPass(buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
        }

        vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);

        {
            std::array<VkBuffer, 1> vertexBuffers = {mVertexBuffer};
            std::array<VkDeviceSize, 1> offsets = {0};

            vkCmdBindVertexBuffers(buffer, 0, 1, vertexBuffers.data(), offsets.data());
        }

        vkCmdDraw(buffer, 6, 1, 0, 0);
        vkCmdEndRenderPass(buffer);
        checkVk(vkEndCommandBuffer(buffer), "Cannot record command buffer");
    }

    // Create semaphores:
    {
        VkSemaphoreCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        checkVk(vkCreateSemaphore(mDevice, &info, nullptr, &mImageAvailableSemaphore), "Cannot create semaphore");
        checkVk(vkCreateSemaphore(mDevice, &info, nullptr, &mRenderFinishedSemaphore), "Cannot create semaphore");
    }
}

Application::~Application()
{
    vkFreeMemory(mDevice, mVertexBufferMemory, nullptr);
    vkDestroyBuffer(mDevice, mVertexBuffer, nullptr);
    vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
    vkDestroySemaphore(mDevice, mImageAvailableSemaphore, nullptr);
    vkDestroySemaphore(mDevice, mRenderFinishedSemaphore, nullptr);
    for (auto &framebuffer : mSwapchainFramebuffers) {
        vkDestroyFramebuffer(mDevice, framebuffer, nullptr);
    }
    vkDestroyPipeline(mDevice, mPipeline, nullptr);
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    vkDestroyRenderPass(mDevice, mRenderPass, nullptr);
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

        vkQueueWaitIdle(mPresentQueue);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(mDevice, mSwapchain, std::numeric_limits<uint64_t>::max(), mImageAvailableSemaphore,
                              VK_NULL_HANDLE, &imageIndex);

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

    vkDeviceWaitIdle(mDevice);
}

#pragma clang diagnostic pop // ignored "-Wreturn-stack-address"

VkBool32
Application::debugCallback(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char *,
        const char *msg, void *)
{
    std::cout << "VK-DEBUG: " << msg << std::endl;
    return VK_FALSE;
}

uint32_t Application::findMemoryType(uint32_t filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps = {};
    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if (filter & (1 << i) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    check(false, "Cannot find suitable memory type");
}
