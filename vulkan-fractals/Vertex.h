#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>

class Vertex
{

private:
    glm::vec2 position;

public:
    Vertex(float x, float y);

    static VkVertexInputBindingDescription getBindingDescription();

    static std::array<VkVertexInputAttributeDescription, 1> getAttributeDescription();

};
