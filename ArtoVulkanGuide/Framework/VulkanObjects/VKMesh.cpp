#include <iostream>
#include <tiny_obj_loader.h>

#include "VKMesh.hpp"

VertexInputDesc Vertex::GetVertexDesc()
{
    VertexInputDesc viDesc;

    VkVertexInputBindingDescription mainBinding = {};
    mainBinding.binding = 0;
    mainBinding.stride = sizeof(Vertex);
    mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    viDesc.mBindings.push_back(mainBinding);

    //Position will be stored at Location 0
    VkVertexInputAttributeDescription positionAttribute = {};
    positionAttribute.binding = 0;
    positionAttribute.location = 0;
    positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    positionAttribute.offset = offsetof(Vertex, mPosition);

    // Normal will be stored at Location 1
    VkVertexInputAttributeDescription normalAttribute = {};
    normalAttribute.binding = 0;
    normalAttribute.location = 1;
    normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    normalAttribute.offset = offsetof(Vertex, mNormal);

    // Position will be stored at Location 2
    VkVertexInputAttributeDescription colorAttribute = {};
    colorAttribute.binding = 0;
    colorAttribute.location = 2;
    colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    colorAttribute.offset = offsetof(Vertex, mColor);

    // UV will be stored at Location 3
    VkVertexInputAttributeDescription uvAttribute = {};
    uvAttribute.binding = 0;
    uvAttribute.location = 3;
    uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
    uvAttribute.offset = offsetof(Vertex, mUV);

    viDesc.mAttributes.push_back(positionAttribute);
    viDesc.mAttributes.push_back(normalAttribute);
    viDesc.mAttributes.push_back(colorAttribute);
    viDesc.mAttributes.push_back(uvAttribute);

    return viDesc;
}

bool Mesh::LoadFromOBJ(const char* filename)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn, err;

    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, nullptr);

    if (!warn.empty())
    {
        std::cout << "WARNING: " << warn << std::endl;
    }

    if (!err.empty())
    {
        std::cerr << "ERROR: " << err << std::endl;
        return false;
    }

    for (auto & shape : shapes)
    {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++)
        {
            int fv = 3;
            for (size_t v = 0; v < fv; v++)
            {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

                // vertex position
                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
                // vertex normal
                tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];
                //vertex uv
                tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
                tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

                // copy to vertex struct
                Vertex newVert{};
                newVert.mPosition = glm::vec3(vx, vy, vz);
                newVert.mNormal = glm::vec3(nx, ny, nz);
                newVert.mColor = newVert.mNormal;
                newVert.mUV = glm::vec2(ux, 1 - uy);

                mVertices.push_back(newVert);
            }
            indexOffset += fv;
        }
    }
    return true;
}