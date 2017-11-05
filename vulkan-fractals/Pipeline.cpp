//
// Created by jim on 11/3/17.
//

#include "Pipeline.h"

Pipeline::Pipeline(const VkPhysicalDevice &physicalDevice, const VkDevice &device)
        : mPhysicalDevice{physicalDevice}
          , mDevice{device}
{
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-stack-address"

void Pipeline::init(VkExtent2D swapchainExtent, VkRenderPass renderPass)
{
    // Create descriptor set layout:
    {
        VkDescriptorSetLayoutBinding uboLayoutBinding = {};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = &uboLayoutBinding;

        checkVk(vkCreateDescriptorSetLayout(mDevice, &info, nullptr, &mDescriptorSetLayout),
                "Cannot create descriptor set layout");
    }

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
    viewport.height = swapchainExtent.height;
    viewport.width = swapchainExtent.width;
    viewport.x = 0;
    viewport.y = 0;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;

    VkRect2D scissor = {};
    scissor.extent = swapchainExtent;
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
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthBiasEnable = VK_FALSE;
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
        info.setLayoutCount = 1;
        info.pSetLayouts = &mDescriptorSetLayout;

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
    info.renderPass = renderPass;
    info.subpass = 0;
    info.pViewportState = &viewportState;

    checkVk(vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &info, nullptr, &mPipeline),
            "Cannot create graphics pipeline");
}

#pragma clang diagnostic pop

void Pipeline::destroy()
{
    vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayout, nullptr);
    vkDestroyPipeline(mDevice, mPipeline, nullptr);
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
    for (auto &module : mShaderModules) {
        vkDestroyShaderModule(mDevice, module, nullptr);
    }
}

const VkPipeline &Pipeline::getHandle() const
{
    return mPipeline;
}

const VkDescriptorSetLayout &Pipeline::getDescriptorSetLayout() const
{
    return mDescriptorSetLayout;
}

const VkPipelineLayout &Pipeline::getLayout() const
{
    return mPipelineLayout;
}
