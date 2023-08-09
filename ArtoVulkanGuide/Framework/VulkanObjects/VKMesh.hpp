#pragma once

#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "VKTypes.hpp"

struct VertexInputDesc
{
    std::vector<VkVertexInputBindingDescription> mBindings;
    std::vector<VkVertexInputAttributeDescription> mAttributes;

    VkPipelineVertexInputStateCreateFlags mFlags = 0;
};

struct Vertex
{
    glm::vec3 mPosition;
    glm::vec3 mNormal;
    glm::vec3 mColor;

    static VertexInputDesc GetVertexDesc();
};

struct Mesh
{
    std::vector<Vertex> mVertices;
    AllocatedBuffer mVertexBuffer;

    bool LoadFromOBJ(const char* filename);
};