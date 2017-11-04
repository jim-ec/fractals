//
// Created by jim on 11/3/17.
//

#pragma once

#include <vulkan/vulkan.h>

#include "util.h"
#include "Vertex.h"

class Pipeline
{

public:

    Pipeline(const VkPhysicalDevice &physicalDevice, const VkDevice &device);

    void init(VkExtent2D swapchainExtent, VkRenderPass renderPass);

    void destroy();

    VkPipeline getHandle() const;

    VkDescriptorSetLayout const &getDescriptorSetLayout() const;

    const VkPipelineLayout &getLayout();

private:

    const VkPhysicalDevice &mPhysicalDevice;
    const VkDevice &mDevice;
    std::array<VkShaderModule, 2> mShaderModules;
    VkDescriptorSetLayout mDescriptorSetLayout;
    VkPipelineLayout mPipelineLayout;
    VkPipeline mPipeline;

};
