#pragma once

#include <VKTypes.h>
#include <vector>
#include <glm/glm.hpp>

struct Vertex
{
    glm::vec2 mUV;
    glm::vec3 mPosition;
    glm::vec3 mNormal;
    glm::vec4 mColor;
};

struct Surface
{
    size_t mIndexCount;
    AllocatedBuffer mIndexBuffer;
    AllocatedBuffer mVertexBuffer;
    VkDescriptorSet mBufferBinding;
};