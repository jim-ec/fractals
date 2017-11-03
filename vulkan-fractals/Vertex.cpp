//
// Created by jim on 11/3/17.
//

#include "Vertex.h"

Vertex::Vertex(float x, float y) : position{x, y}
{
}

VkVertexInputBindingDescription Vertex::getBindingDescription()
{
    VkVertexInputBindingDescription binding = {};
    binding.binding = 0;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding.stride = sizeof(Vertex);
    return binding;
}

std::array<VkVertexInputAttributeDescription, 1> Vertex::getAttributeDescription()
{
    std::array<VkVertexInputAttributeDescription, 1> attributes = {};
    attributes[0].binding = 0;
    attributes[0].offset = static_cast<uint32_t>(offsetof(Vertex, position));
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    return attributes;
}
