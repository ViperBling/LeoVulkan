﻿#include "AssetsLoader.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

namespace LeoVK
{

    BoundingBox::BoundingBox() {}

    BoundingBox::BoundingBox(glm::vec3 min, glm::vec3 max) : mMin(min), mMax(max) {}

    BoundingBox BoundingBox::GetAABB(glm::mat4 mat)
    {
        auto min = glm::vec3(mat[3]);
        glm::vec3 max = min;
        glm::vec3 v0, v1;

        auto right = glm::vec3(mat[0]);
        v0 = right * this->mMin.x;
        v1 = right * this->mMax.x;
        min += glm::min(v0, v1);
        max += glm::max(v0, v1);

        auto up = glm::vec3(mat[1]);
        v0 = up * this->mMin.y;
        v1 = up * this->mMax.y;
        min += glm::min(v0, v1);
        max += glm::max(v0, v1);

        auto back = glm::vec3(mat[2]);
        v0 = back * this->mMin.z;
        v1 = back * this->mMax.z;
        min += glm::min(v0, v1);
        max += glm::max(v0, v1);

        return {min, max};
    }

    void Primitive::SetBoundingBox(glm::vec3 min, glm::vec3 max)
    {
        mBBox.mMin = min;
        mBBox.mMax = max;
        mBBox.mbValid = true;
    }

    Mesh::Mesh(LeoVK::VulkanDevice *device, glm::mat4 matrix)
    {
        this->mpDevice = device;
        this->mUniformBlock.mMatrix = matrix;
        VK_CHECK(device->CreateBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            sizeof(mUniformBlock),
            &mUniformBuffer.mBuffer,
            &mUniformBuffer.mMemory,
            &mUniformBlock));
        VK_CHECK(vkMapMemory(
            mpDevice->mLogicalDevice,
            mUniformBuffer.mMemory, 0,
            sizeof(mUniformBlock), 0,
            &mUniformBuffer.mpMapped));
        mUniformBuffer.mDescriptor = {mUniformBuffer.mBuffer, 0, sizeof(mUniformBlock)};
    }

    Mesh::~Mesh()
    {
        vkDestroyBuffer(mpDevice->mLogicalDevice, mUniformBuffer.mBuffer, nullptr);
        vkFreeMemory(mpDevice->mLogicalDevice, mUniformBuffer.mMemory, nullptr);
        for (auto primitive : mPrimitives) delete primitive;
    }

    void Mesh::SetBoundingBox(glm::vec3 min, glm::vec3 max)
    {
        mAABB.mMin = min;
        mAABB.mMax = max;
        mAABB.mbValid = true;
    }

    glm::mat4 Node::LocalMatrix()
    {
        return glm::translate(
            glm::mat4(1.0f), mTranslation) *
            glm::mat4(mRotation) *
            glm::scale(glm::mat4(1.0f), mScale) * mMatrix;
    }

    glm::mat4 Node::GetMatrix()
    {
        glm::mat4 m = LocalMatrix();
        LeoVK::Node *p = mpParent;
        while (p)
        {
            m = p->LocalMatrix() * m;
            p = p->mpParent;
        }
        return m;
    }

    void Node::Update()
    {
        if (mpMesh)
        {
            glm::mat4 m = GetMatrix();
            if (mpSkin)
            {
                mpMesh->mUniformBlock.mMatrix = m;
                // Update join matrices
                glm::mat4 inverseTransform = glm::inverse(m);
                size_t numJoints = std::min((uint32_t)mpSkin->mJoints.size(), MAX_NUM_JOINTS);
                // 对包含Skin的每个结点应用变换
                for (size_t i = 0; i < numJoints; i++)
                {
                    LeoVK::Node* jointNode = mpSkin->mJoints[i];
                    glm::mat4 jointMat = jointNode->GetMatrix() * mpSkin->mInverseBindMatrices[i];
                    jointMat = inverseTransform * jointMat;
                    mpMesh->mUniformBlock.mJointMatrix[i] = jointMat;
                }

                mpMesh->mUniformBlock.mJointCount = (float)numJoints;
                memcpy(mpMesh->mUniformBuffer.mpMapped, &mpMesh->mUniformBlock, sizeof(mpMesh->mUniformBlock));
            }
            else
            {
                memcpy(mpMesh->mUniformBuffer.mpMapped, &m, sizeof(glm::mat4));
            }
        }

        for (auto& child : mChildren) child->Update();
    }

    Node::~Node()
    {
        if (mpMesh) delete mpMesh;
        for (auto& child : mChildren) delete child;
    }

    void GLTFScene::Destroy(VkDevice deivce)
    {
        if (mVertices.mBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(deivce, mVertices.mBuffer, nullptr);
            vkFreeMemory(deivce, mVertices.mMemory, nullptr);
            mVertices.mBuffer = VK_NULL_HANDLE;
        }
        if (mIndices.mBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(deivce, mIndices.mBuffer, nullptr);
            vkFreeMemory(deivce, mIndices.mMemory, nullptr);
            mIndices.mBuffer = VK_NULL_HANDLE;
        }

        for (auto texture : mTextures) texture.Destroy();

        mTextures.resize(0);
        mTexSamplers.resize(0);

        for (auto node : mNodes) delete node;

        mMaterials.resize(0);
        mAnimations.resize(0);
        mNodes.resize(0);
        mLinearNodes.resize(0);
        mExtensions.resize(0);

        for (auto skin : mSkins) delete skin;

        mSkins.resize(0);
    }

    void GLTFScene::LoadNode(
        LeoVK::Node *parent,
        const tinygltf::Node &node,
        uint32_t nodeIndex,
        const tinygltf::Model &model,
        LoaderInfo &loaderInfo,
        float globalScale)
    {
        LeoVK::Node* newNode = new Node{};
        newNode->mIndex = nodeIndex;
        newNode->mpParent = parent;
        newNode->mName = node.name;
        newNode->mSkinIndex = node.skin;
        newNode->mMatrix = glm::mat4(1.0f);

        // Generate local node matrix
        auto translation = glm::vec3(0.0f);
        if (node.translation.size() == 3)
        {
            translation = glm::make_vec3(node.translation.data());
            newNode->mTranslation = translation;
        }
        glm::mat4 rotation = glm::mat4(1.0f);
        if (node.rotation.size() == 4)
        {
            glm::quat q = glm::make_quat(node.rotation.data());
            newNode->mRotation = glm::mat4(q);
        }
        auto scale = glm::vec3(1.0f);
        if (node.scale.size() == 3)
        {
            scale = glm::make_vec3(node.scale.data());
            newNode->mScale = scale;
        }
        if (node.matrix.size() == 16)
        {
            newNode->mMatrix = glm::make_mat4x4(node.matrix.data());
        }

        // Node with children
        if (!node.children.empty())
        {
            for (int i : node.children)
            {
                LoadNode(newNode, model.nodes[i], i, model, loaderInfo, globalScale);
            }
        }

        // Node contains mesh data
        if (node.mesh > -1)
        {
            const tinygltf::Mesh mesh = model.meshes[node.mesh];
            Mesh *newMesh = new Mesh(mpDevice, newNode->mMatrix);

            for (const auto & primitive : mesh.primitives)
            {
                auto vertexStart = static_cast<uint32_t>(loaderInfo.mVertexPos);
                auto indexStart = static_cast<uint32_t>(loaderInfo.mIndexPos);
                uint32_t indexCount = 0;
                uint32_t vertexCount = 0;
                glm::vec3 posMin{};
                glm::vec3 posMax{};
                bool hasSkin = false;
                bool hasIndices = primitive.indices > -1;

                // Vertices
                {
                    const float* bufferPos = nullptr;
                    const float* bufferNormals = nullptr;
                    const float* bufferTexCoordSet0 = nullptr;
                    const float* bufferTexCoordSet1 = nullptr;
                    const float* bufferColorSet0 = nullptr;
                    const void * bufferJoints = nullptr;
                    const float* bufferWeights = nullptr;

                    uint32_t posByteStride;
                    uint32_t normByteStride;
                    uint32_t uv0ByteStride;
                    uint32_t uv1ByteStride;
                    uint32_t color0ByteStride;
                    uint32_t jointByteStride;
                    uint32_t weightByteStride;

                    int jointComponentType;

                    // Position attribute is required
                    assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

                    const tinygltf::Accessor &posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
                    const tinygltf::BufferView &posView = model.bufferViews[posAccessor.bufferView];

                    bufferPos = reinterpret_cast<const float *>(&(model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
                    posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
                    posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);
                    vertexCount = static_cast<uint32_t>(posAccessor.count);
                    posByteStride = posAccessor.ByteStride(posView) ? (posAccessor.ByteStride(posView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);

                    if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
                    {
                        const tinygltf::Accessor &normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
                        const tinygltf::BufferView &normView = model.bufferViews[normAccessor.bufferView];
                        bufferNormals = reinterpret_cast<const float *>(&(model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
                        normByteStride = normAccessor.ByteStride(normView) ? (normAccessor.ByteStride(normView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
                    }

                    // UVs
                    if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
                    {
                        const tinygltf::Accessor &uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
                        const tinygltf::BufferView &uvView = model.bufferViews[uvAccessor.bufferView];
                        bufferTexCoordSet0 = reinterpret_cast<const float *>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
                        uv0ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
                    }
                    if (primitive.attributes.find("TEXCOORD_1") != primitive.attributes.end())
                    {
                        const tinygltf::Accessor &uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_1")->second];
                        const tinygltf::BufferView &uvView = model.bufferViews[uvAccessor.bufferView];
                        bufferTexCoordSet1 = reinterpret_cast<const float *>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
                        uv1ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
                    }

                    // Vertex colors
                    if (primitive.attributes.find("COLOR_0") != primitive.attributes.end())
                    {
                        const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.find("COLOR_0")->second];
                        const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                        bufferColorSet0 = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                        color0ByteStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
                    }

                    // Skinning
                    // Joints
                    if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end())
                    {
                        const tinygltf::Accessor &jointAccessor = model.accessors[primitive.attributes.find("JOINTS_0")->second];
                        const tinygltf::BufferView &jointView = model.bufferViews[jointAccessor.bufferView];
                        bufferJoints = &(model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]);
                        jointComponentType = jointAccessor.componentType;
                        jointByteStride = jointAccessor.ByteStride(jointView) ? (jointAccessor.ByteStride(jointView) / tinygltf::GetComponentSizeInBytes(jointComponentType)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
                    }

                    if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end())
                    {
                        const tinygltf::Accessor &weightAccessor = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
                        const tinygltf::BufferView &weightView = model.bufferViews[weightAccessor.bufferView];
                        bufferWeights = reinterpret_cast<const float *>(&(model.buffers[weightView.buffer].data[weightAccessor.byteOffset + weightView.byteOffset]));
                        weightByteStride = weightAccessor.ByteStride(weightView) ? (weightAccessor.ByteStride(weightView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
                    }

                    hasSkin = (bufferJoints && bufferWeights);

                    for (size_t v = 0; v < posAccessor.count; v++)
                    {
                        Vertex& vert = loaderInfo.mpVertexBuffer[loaderInfo.mVertexPos];
                        vert.mPos = glm::vec4(glm::make_vec3(&bufferPos[v * posByteStride]), 1.0f);
                        vert.mNormal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * normByteStride]) : glm::vec3(0.0f)));
                        vert.mUV0 = bufferTexCoordSet0 ? glm::make_vec2(&bufferTexCoordSet0[v * uv0ByteStride]) : glm::vec3(0.0f);
                        vert.mUV1 = bufferTexCoordSet1 ? glm::make_vec2(&bufferTexCoordSet1[v * uv1ByteStride]) : glm::vec3(0.0f);
                        vert.mColor = bufferColorSet0 ? glm::make_vec4(&bufferColorSet0[v * color0ByteStride]) : glm::vec4(1.0f);

                        if (hasSkin)
                        {
                            switch (jointComponentType)
                            {
                                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                                {
                                    auto buf = static_cast<const uint16_t*>(bufferJoints);
                                    vert.mJoint0 = glm::vec4(glm::make_vec4(&buf[v * jointByteStride]));
                                    break;
                                }
                                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                                {
                                    auto buf = static_cast<const uint8_t*>(bufferJoints);
                                    vert.mJoint0 = glm::vec4(glm::make_vec4(&buf[v * jointByteStride]));
                                    break;
                                }
                                default:
                                    // Not supported by spec
                                    std::cerr << "Joint component type " << jointComponentType << " not supported!" << std::endl;
                                    break;
                            }
                        }
                        else
                        {
                            vert.mJoint0 = glm::vec4(0.0f);
                        }
                        vert.mWeight0 = hasSkin ? glm::make_vec4(&bufferWeights[v * weightByteStride]) : glm::vec4(0.0f);
                        // Fix for all zero weights
                        if (glm::length(vert.mWeight0) == 0.0f)
                        {
                            vert.mWeight0 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                        }
                        loaderInfo.mVertexPos++;
                    }
                }
                // Indices
                if (hasIndices)
                {
                    const tinygltf::Accessor &accessor = model.accessors[primitive.indices > -1 ? primitive.indices : 0];
                    const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

                    indexCount = static_cast<uint32_t>(accessor.count);
                    const void *dataPtr = &(buffer.data[accessor.byteOffset + bufferView.byteOffset]);

                    switch (accessor.componentType)
                    {
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                        {
                            auto buf = static_cast<const uint32_t*>(dataPtr);
                            for (size_t index = 0; index < accessor.count; index++)
                            {
                                loaderInfo.mpIndexBuffer[loaderInfo.mIndexPos] = buf[index] + vertexStart;
                                loaderInfo.mIndexPos++;
                            }
                            break;
                        }
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                        {
                            auto buf = static_cast<const uint16_t*>(dataPtr);
                            for (size_t index = 0; index < accessor.count; index++)
                            {
                                loaderInfo.mpIndexBuffer[loaderInfo.mIndexPos] = buf[index] + vertexStart;
                                loaderInfo.mIndexPos++;
                            }
                            break;
                        }
                        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                        {
                            auto buf = static_cast<const uint8_t*>(dataPtr);
                            for (size_t index = 0; index < accessor.count; index++)
                            {
                                loaderInfo.mpIndexBuffer[loaderInfo.mIndexPos] = buf[index] + vertexStart;
                                loaderInfo.mIndexPos++;
                            }
                            break;
                        }
                        default:
                            std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
                            return;
                    }
                }
                auto * newPrimitive = new Primitive(indexStart, indexCount, vertexCount, primitive.material > -1 ? mMaterials[primitive.material] : mMaterials.back());
                newPrimitive->SetBoundingBox(posMin, posMax);
                newMesh->mPrimitives.push_back(newPrimitive);
            }

            // Mesh BB from BBs of primitives
            for (auto p : newMesh->mPrimitives)
            {
                if (p->mBBox.mbValid && !newMesh->mBBox.mbValid)
                {
                    newMesh->mBBox = p->mBBox;
                    newMesh->mBBox.mbValid = true;
                }
                newMesh->mBBox.mMin = glm::min(newMesh->mBBox.mMin, p->mBBox.mMin);
                newMesh->mBBox.mMax = glm::max(newMesh->mBBox.mMax, p->mBBox.mMax);
            }
            newNode->mpMesh = newMesh;
        }
        if (parent)
        {
            parent->mChildren.push_back(newNode);
        } else
        {
            mNodes.push_back(newNode);
        }
        mLinearNodes.push_back(newNode);
    }

    void GLTFScene::GetNodeProperty(
        const tinygltf::Node& node,
        const tinygltf::Model& model,
        size_t& vertexCount,
        size_t& indexCount)
    {
        if (!node.children.empty())
        {
            for (int i : node.children)
            {
                GetNodeProperty(model.nodes[i], model, vertexCount, indexCount);
            }
        }
        if (node.mesh > -1)
        {
            const tinygltf::Mesh mesh = model.meshes[node.mesh];

            for (auto primitive : mesh.primitives)
            {
                vertexCount += model.accessors[primitive.attributes.find("POSITION")->second].count;
                if (primitive.indices > -1) indexCount += model.accessors[primitive.indices].count;
            }
        }
    }

    void GLTFScene::LoadSkins(tinygltf::Model &gltfModel)
    {
        for (tinygltf::Skin &source : gltfModel.skins)
        {
            Skin *newSkin = new Skin{};
            newSkin->mName = source.name;

            // Find skeleton root node
            if (source.skeleton > -1)
            {
                newSkin->mpSkeletonRoot = NodeFromIndex(source.skeleton);
            }

            // Find joint nodes
            for (int jointIndex : source.joints)
            {
                Node* node = NodeFromIndex(jointIndex);
                if (node) newSkin->mJoints.push_back(NodeFromIndex(jointIndex));

            }

            // Get inverse bind matrices from buffer
            if (source.inverseBindMatrices > -1)
            {
                const tinygltf::Accessor &accessor = gltfModel.accessors[source.inverseBindMatrices];
                const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];

                newSkin->mInverseBindMatrices.resize(accessor.count);
                memcpy(newSkin->mInverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::mat4));
            }

            mSkins.push_back(newSkin);
        }
    }

    void GLTFScene::LoadTextures(
        tinygltf::Model &gltfModel,
        LeoVK::VulkanDevice *device,
        VkQueue transferQueue)
    {
        for (tinygltf::Texture &tex : gltfModel.textures)
        {
            tinygltf::Image image = gltfModel.images[tex.source];
            LeoVK::TextureSampler texSampler{};
            if (tex.sampler == -1)
            {
                // No sampler specified, use a default one
                texSampler.mMagFilter = VK_FILTER_LINEAR;
                texSampler.mMinFilter = VK_FILTER_LINEAR;
                texSampler.mAddressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                texSampler.mAddressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                texSampler.mAddressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            }
            else
            {
                texSampler = mTexSamplers[tex.sampler];
            }

            LeoVK::Texture2D texture;
            texture.LoadFromImage(image, texSampler, device, transferQueue);
            mTextures.push_back(texture);
        }
    }

    VkSamplerAddressMode GLTFScene::GetVkWrapMode(int32_t wrapMode)
    {
        switch (wrapMode)
        {
            case -1:
            case 10497:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case 33071:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            case 33648:
                return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            default:
                break;
        }

        std::cerr << "Unknown wrap mode for getVkWrapMode: " << wrapMode << std::endl;
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }

    VkFilter GLTFScene::GetVkFilterMode(int32_t filterMode)
    {
        switch (filterMode)
        {
            case -1:
            case 9728:
                return VK_FILTER_NEAREST;
            case 9729:
                return VK_FILTER_LINEAR;
            case 9984:
                return VK_FILTER_NEAREST;
            case 9985:
                return VK_FILTER_NEAREST;
            case 9986:
                return VK_FILTER_LINEAR;
            case 9987:
                return VK_FILTER_LINEAR;
            default:
                break;
        }

        std::cerr << "Unknown filter mode for getVkFilterMode: " << filterMode << std::endl;
        return VK_FILTER_NEAREST;
    }

    void GLTFScene::LoadTextureSamplers(tinygltf::Model &gltfModel)
    {
        for (tinygltf::Sampler& smpl : gltfModel.samplers)
        {
            LeoVK::TextureSampler sampler{};
            sampler.mMinFilter = GetVkFilterMode(smpl.minFilter);
            sampler.mMagFilter = GetVkFilterMode(smpl.magFilter);
            sampler.mAddressModeU = GetVkWrapMode(smpl.wrapS);
            sampler.mAddressModeV = GetVkWrapMode(smpl.wrapT);
            sampler.mAddressModeW = sampler.mAddressModeV;
            mTexSamplers.push_back(sampler);
        }
    }

    void GLTFScene::LoadMaterials(tinygltf::Model &gltfModel)
    {
        for (tinygltf::Material &mat : gltfModel.materials)
        {
            LeoVK::Material material{};
            material.mbDoubleSided = mat.doubleSided;

            if (mat.values.find("baseColorTexture") != mat.values.end())
            {
                material.mpBaseColorTexture = &mTextures[mat.values["baseColorTexture"].TextureIndex()];
                material.mTexCoordSets.mBaseColor = mat.values["baseColorTexture"].TextureTexCoord();
            }
            if (mat.values.find("metallicRoughnessTexture") != mat.values.end())
            {
                material.mpMetallicRoughnessTexture = &mTextures[mat.values["metallicRoughnessTexture"].TextureIndex()];
                material.mTexCoordSets.mMetallicRoughness = mat.values["metallicRoughnessTexture"].TextureTexCoord();
            }
            if (mat.values.find("roughnessFactor") != mat.values.end())
            {
                material.mRoughnessFactor = static_cast<float>(mat.values["roughnessFactor"].Factor());
            }
            if (mat.values.find("metallicFactor") != mat.values.end())
            {
                material.mMetallicFactor = static_cast<float>(mat.values["metallicFactor"].Factor());
            }
            if (mat.values.find("baseColorFactor") != mat.values.end())
            {
                material.mBaseColorFactor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
            }
            if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end())
            {
                material.mpNormalTexture = &mTextures[mat.additionalValues["normalTexture"].TextureIndex()];
                material.mTexCoordSets.mNormal = mat.additionalValues["normalTexture"].TextureTexCoord();
            }
            if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end())
            {
                material.mpEmissiveTexture = &mTextures[mat.additionalValues["emissiveTexture"].TextureIndex()];
                material.mTexCoordSets.mEmissive = mat.additionalValues["emissiveTexture"].TextureTexCoord();
            }
            if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end())
            {
                material.mpOcclusionTexture = &mTextures[mat.additionalValues["occlusionTexture"].TextureIndex()];
                material.mTexCoordSets.mOcclusion = mat.additionalValues["occlusionTexture"].TextureTexCoord();
            }
            if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end())
            {
                tinygltf::Parameter param = mat.additionalValues["alphaMode"];
                if (param.string_value == "BLEND")
                {
                    material.mAlphaMode = Material::ALPHA_MODE_BLEND;
                }
                if (param.string_value == "MASK")
                {
                    material.mAlphaCutoff = 0.5f;
                    material.mAlphaMode = Material::ALPHA_MODE_MASK;
                }
            }
            if (mat.additionalValues.find("alphaCutoff") != mat.additionalValues.end()) {
                material.mAlphaCutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
            }
            if (mat.additionalValues.find("emissiveFactor") != mat.additionalValues.end())
            {
                material.mEmissiveFactor = glm::vec4(glm::make_vec3(mat.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0);
            }

            // Extensions
            // @TODO: Find out if there is a nicer way of reading these properties with recent tinygltf headers
            if (mat.extensions.find("KHR_materials_pbrSpecularGlossiness") != mat.extensions.end())
            {
                auto ext = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");

                if (ext->second.Has("specularGlossinessTexture"))
                {
                    auto index = ext->second.Get("specularGlossinessTexture").Get("index");
                    material.mExtension.mpSpecularGlossinessTexture = &mTextures[index.Get<int>()];
                    auto texCoordSet = ext->second.Get("specularGlossinessTexture").Get("texCoord");
                    material.mTexCoordSets.mSpecularGlossiness = texCoordSet.Get<int>();
                    material.mPBRWorkFlows.mbSpecularGlossiness = true;
                }
                if (ext->second.Has("diffuseTexture"))
                {
                    auto index = ext->second.Get("diffuseTexture").Get("index");
                    material.mExtension.mpDiffuseTexture = &mTextures[index.Get<int>()];
                }
                if (ext->second.Has("diffuseFactor"))
                {
                    auto factor = ext->second.Get("diffuseFactor");
                    for (uint32_t i = 0; i < factor.ArrayLen(); i++)
                    {
                        auto val = factor.Get(i);
                        material.mExtension.mDiffuseFactor[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
                    }
                }
                if (ext->second.Has("specularFactor"))
                {
                    auto factor = ext->second.Get("specularFactor");
                    for (uint32_t i = 0; i < factor.ArrayLen(); i++)
                    {
                        auto val = factor.Get(i);
                        material.mExtension.mSpecularFactor[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
                    }
                }
            }
            mMaterials.push_back(material);
        }
        // Push a default material at the end of the list for meshes with no material assigned
        mMaterials.emplace_back();
    }

    void GLTFScene::LoadAnimations(tinygltf::Model &gltfModel)
    {
        for (tinygltf::Animation &anim : gltfModel.animations)
        {
            LeoVK::Animation animation{};
            animation.mName = anim.name;

            if (anim.name.empty())
            {
                animation.mName = std::to_string(mAnimations.size());
            }

            // Samplers
            for (auto &smpl : anim.samplers)
            {
                LeoVK::AnimationSampler sampler{};

                if (smpl.interpolation == "LINEAR")
                {
                    sampler.mInterpolation = AnimationSampler::InterpolationType::LINEAR;
                }
                if (smpl.interpolation == "STEP")
                {
                    sampler.mInterpolation = AnimationSampler::InterpolationType::STEP;
                }
                if (smpl.interpolation == "CUBICSPLINE")
                {
                    sampler.mInterpolation = AnimationSampler::InterpolationType::CUBICSPLINE;
                }

                // Read sampler input time values
                {
                    const tinygltf::Accessor &accessor = gltfModel.accessors[smpl.input];
                    const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];

                    assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

                    const void *dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
                    auto *buf = static_cast<const float*>(dataPtr);
                    for (size_t index = 0; index < accessor.count; index++)
                    {
                        sampler.mInputs.push_back(buf[index]);
                    }

                    for (auto input : sampler.mInputs)
                    {
                        if (input < animation.mStart)
                        {
                            animation.mStart = input;
                        };
                        if (input > animation.mEnd)
                        {
                            animation.mEnd = input;
                        }
                    }
                }

                // Read sampler output T/R/S values
                {
                    const tinygltf::Accessor &accessor = gltfModel.accessors[smpl.output];
                    const tinygltf::BufferView &bufferView = gltfModel.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer &buffer = gltfModel.buffers[bufferView.buffer];

                    assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);
                    const void *dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

                    switch (accessor.type)
                    {
                        case TINYGLTF_TYPE_VEC3:
                        {
                            auto *buf = static_cast<const glm::vec3*>(dataPtr);
                            for (size_t index = 0; index < accessor.count; index++)
                            {
                                sampler.mOutputsVec4.push_back(glm::vec4(buf[index], 0.0f));
                            }
                            break;
                        }
                        case TINYGLTF_TYPE_VEC4:
                        {
                            auto *buf = static_cast<const glm::vec4*>(dataPtr);
                            for (size_t index = 0; index < accessor.count; index++)
                            {
                                sampler.mOutputsVec4.push_back(buf[index]);
                            }
                            break;
                        }
                        default: {
                            std::cout << "unknown type" << std::endl;
                            break;
                        }
                    }
                }

                animation.mSamplers.push_back(sampler);
            }

            // Channels
            for (auto &source: anim.channels)
            {
                LeoVK::AnimationChannel channel{};

                if (source.target_path == "rotation")
                {
                    channel.mPath = AnimationChannel::PathType::ROTATION;
                }
                if (source.target_path == "translation")
                {
                    channel.mPath = AnimationChannel::PathType::TRANSLATION;
                }
                if (source.target_path == "scale")
                {
                    channel.mPath = AnimationChannel::PathType::SCALE;
                }
                if (source.target_path == "weights")
                {
                    std::cout << "weights not yet supported, skipping channel" << std::endl;
                    continue;
                }
                channel.mSamplerIndex = source.sampler;
                channel.mpNode = NodeFromIndex(source.target_node);
                if (!channel.mpNode) continue;

                animation.mChannels.push_back(channel);
            }
            mAnimations.push_back(animation);
        }
    }

    void GLTFScene::LoadFromFile(
        std::string &filename,
        LeoVK::VulkanDevice *device,
        VkQueue transferQueue,
        float scale)
    {
        tinygltf::Model gltfModel;
        tinygltf::TinyGLTF gltfContext;

        std::string error;
        std::string warning;

        this->mpDevice = device;

        bool binary = false;
        size_t extpos = filename.rfind('.', filename.length());
        if (extpos != std::string::npos)
        {
            binary = (filename.substr(extpos + 1, filename.length() - extpos) == "glb");
        }

        bool fileLoaded = binary ? gltfContext.LoadBinaryFromFile(&gltfModel, &error, &warning, filename) : gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, filename);

        LoaderInfo loaderInfo{};
        size_t vertexCount = 0;
        size_t indexCount = 0;

        if (fileLoaded)
        {
            LoadTextureSamplers(gltfModel);
            LoadTextures(gltfModel, device, transferQueue);
            LoadMaterials(gltfModel);

            const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];

            // Get vertex and index buffer sizes up-front
            for (int node : scene.nodes)
            {
                GetNodeProperty(gltfModel.nodes[node], gltfModel, vertexCount, indexCount);
            }
            loaderInfo.mpVertexBuffer = new Vertex[vertexCount];
            loaderInfo.mpIndexBuffer = new uint32_t[indexCount];

            // TODO: scene handling with no default scene
            for (int i : scene.nodes)
            {
                const tinygltf::Node node = gltfModel.nodes[i];
                LoadNode(nullptr, node, i, gltfModel, loaderInfo, scale);
            }
            if (!gltfModel.animations.empty())
            {
                LoadAnimations(gltfModel);
            }
            LoadSkins(gltfModel);

            for (auto node : mLinearNodes)
            {
                // Assign skins
                if (node->mSkinIndex > -1) node->mpSkin = mSkins[node->mSkinIndex];

                // Initial pose
                if (node->mpMesh) node->Update();

            }
        }
        else
        {
            // TODO: throw
            std::cerr << "Could not load gltf file: " << error << std::endl;
            return;
        }

        mExtensions = gltfModel.extensionsUsed;

        size_t vertexBufferSize = vertexCount * sizeof(Vertex);
        size_t indexBufferSize = indexCount * sizeof(uint32_t);

        assert(vertexBufferSize > 0);

        struct StagingBuffer
        {
            VkBuffer buffer;
            VkDeviceMemory memory;
        } vertexStaging{}, indexStaging{};

        // Create staging buffers
        // Vertex data
        VK_CHECK(device->CreateBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            vertexBufferSize,
            &vertexStaging.buffer,
            &vertexStaging.memory,
            loaderInfo.mpVertexBuffer));
        // Index data
        if (indexBufferSize > 0)
        {
            VK_CHECK(device->CreateBuffer(
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                indexBufferSize,
                &indexStaging.buffer,
                &indexStaging.memory,
                loaderInfo.mpIndexBuffer));
        }

        // Create device local buffers
        // Vertex buffer
        VK_CHECK(device->CreateBuffer(
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vertexBufferSize,
            &mVertices.mBuffer,
            &mVertices.mMemory));
        // Index buffer
        if (indexBufferSize > 0)
        {
            VK_CHECK(device->CreateBuffer(
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                indexBufferSize,
                &mIndices.mBuffer,
                &mIndices.mMemory));
        }

        // Copy from staging buffers
        VkCommandBuffer copyCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        VkBufferCopy copyRegion = {};

        copyRegion.size = vertexBufferSize;
        vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, mVertices.mBuffer, 1, &copyRegion);

        if (indexBufferSize > 0)
        {
            copyRegion.size = indexBufferSize;
            vkCmdCopyBuffer(copyCmd, indexStaging.buffer, mIndices.mBuffer, 1, &copyRegion);
        }

        device->FlushCommandBuffer(copyCmd, transferQueue, true);

        vkDestroyBuffer(mpDevice->mLogicalDevice, vertexStaging.buffer, nullptr);
        vkFreeMemory(mpDevice->mLogicalDevice, vertexStaging.memory, nullptr);
        if (indexBufferSize > 0)
        {
            vkDestroyBuffer(mpDevice->mLogicalDevice, indexStaging.buffer, nullptr);
            vkFreeMemory(mpDevice->mLogicalDevice, indexStaging.memory, nullptr);
        }

        delete[] loaderInfo.mpVertexBuffer;
        delete[] loaderInfo.mpIndexBuffer;

        GetSceneDimensions();
    }

    void GLTFScene::DrawNode(Node *node, VkCommandBuffer commandBuffer)
    {
        if (node->mpMesh)
        {
            for (Primitive *primitive : node->mpMesh->mPrimitives)
            {
                vkCmdDrawIndexed(commandBuffer, primitive->mIndexCount, 1, primitive->mFirstIndex, 0, 0);
            }
        }
        for (auto& child : node->mChildren)
        {
            DrawNode(child, commandBuffer);
        }
    }

    void GLTFScene::Draw(VkCommandBuffer commandBuffer)
    {
        const VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &mVertices.mBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffer, mIndices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
        for (auto& node : mNodes)
        {
            DrawNode(node, commandBuffer);
        }
    }

    void GLTFScene::CalculateBoundingBox(Node *node, Node *parent)
    {
        BoundingBox parentBvh = parent ? parent->mBVH : BoundingBox(mDimensions.mMin, mDimensions.mMax);

        if (node->mpMesh)
        {
            if (node->mpMesh->mBBox.mbValid)
            {
                node->mAABB = node->mpMesh->mBBox.GetAABB(node->GetMatrix());
                if (node->mChildren.empty())
                {
                    node->mBVH.mMin = node->mAABB.mMin;
                    node->mBVH.mMax = node->mAABB.mMax;
                    node->mBVH.mbValid = true;
                }
            }
        }

        parentBvh.mMin = glm::min(parentBvh.mMin, node->mBVH.mMin);
        parentBvh.mMax = glm::min(parentBvh.mMax, node->mBVH.mMax);

        for (auto &child : node->mChildren)
        {
            CalculateBoundingBox(child, node);
        }
    }

    void GLTFScene::GetSceneDimensions()
    {
        // Calculate binary volume hierarchy for all nodes in the scene
        for (auto node : mLinearNodes)
        {
            CalculateBoundingBox(node, nullptr);
        }

        mDimensions.mMin = glm::vec3(FLT_MAX);
        mDimensions.mMax = glm::vec3(-FLT_MAX);

        for (auto node : mLinearNodes)
        {
            if (node->mBVH.mbValid)
            {
                mDimensions.mMin = glm::min(mDimensions.mMin, node->mBVH.mMin);
                mDimensions.mMax = glm::max(mDimensions.mMax, node->mBVH.mMax);
            }
        }

        // Calculate scene aabb
        mAABB = glm::scale(
            glm::mat4(1.0f),
            glm::vec3(
                mDimensions.mMax[0] - mDimensions.mMin[0],
                mDimensions.mMax[1] - mDimensions.mMin[1],
                mDimensions.mMax[2] - mDimensions.mMin[2]));
        mAABB[3][0] = mDimensions.mMin[0];
        mAABB[3][1] = mDimensions.mMin[1];
        mAABB[3][2] = mDimensions.mMin[2];
    }

    void GLTFScene::UpdateAnimation(uint32_t index, float time)
    {
        if (mAnimations.empty())
        {
            std::cout << ".glTF does not contain animation." << std::endl;
            return;
        }
        if (index > static_cast<uint32_t>(mAnimations.size()) - 1) {
            std::cout << "No animation with index " << index << std::endl;
            return;
        }
        Animation &animation = mAnimations[index];

        bool updated = false;
        for (auto& channel : animation.mChannels)
        {
            LeoVK::AnimationSampler &sampler = animation.mSamplers[channel.mSamplerIndex];
            if (sampler.mInputs.size() > sampler.mOutputsVec4.size())
            {
                continue;
            }

            for (size_t i = 0; i < sampler.mInputs.size() - 1; i++)
            {
                if ((time >= sampler.mInputs[i]) && (time <= sampler.mInputs[i + 1]))
                {
                    float u = std::max(0.0f, time - sampler.mInputs[i]) / (sampler.mInputs[i + 1] - sampler.mInputs[i]);
                    if (u <= 1.0f)
                    {
                        switch (channel.mPath)
                        {
                            case LeoVK::AnimationChannel::PathType::TRANSLATION:
                            {
                                glm::vec4 trans = glm::mix(sampler.mOutputsVec4[i], sampler.mOutputsVec4[i + 1], u);
                                channel.mpNode->mTranslation = glm::vec3(trans);
                                break;
                            }
                            case LeoVK::AnimationChannel::PathType::SCALE:
                            {
                                glm::vec4 trans = glm::mix(sampler.mOutputsVec4[i], sampler.mOutputsVec4[i + 1], u);
                                channel.mpNode->mScale = glm::vec3(trans);
                                break;
                            }
                            case LeoVK::AnimationChannel::PathType::ROTATION:
                            {
                                glm::quat q1;
                                q1.x = sampler.mOutputsVec4[i].x;
                                q1.y = sampler.mOutputsVec4[i].y;
                                q1.z = sampler.mOutputsVec4[i].z;
                                q1.w = sampler.mOutputsVec4[i].w;
                                glm::quat q2;
                                q2.x = sampler.mOutputsVec4[i + 1].x;
                                q2.y = sampler.mOutputsVec4[i + 1].y;
                                q2.z = sampler.mOutputsVec4[i + 1].z;
                                q2.w = sampler.mOutputsVec4[i + 1].w;
                                channel.mpNode->mRotation = glm::normalize(glm::slerp(q1, q2, u));
                                break;
                            }
                        }
                        updated = true;
                    }
                }
            }
        }
        if (updated)
        {
            for (auto &node : mNodes) node->Update();
        }
    }

    Node *GLTFScene::FindNode(Node *parent, uint32_t index)
    {
        Node* nodeFound = nullptr;
        if (parent->mIndex == index)
        {
            return parent;
        }
        for (auto& child : parent->mChildren)
        {
            nodeFound = FindNode(child, index);
            if (nodeFound) break;
        }
        return nodeFound;
    }

    Node *GLTFScene::NodeFromIndex(uint32_t index)
    {
        Node* nodeFound = nullptr;
        for (auto &node : mNodes)
        {
            nodeFound = FindNode(node, index);
            if (nodeFound) break;
        }
        return nodeFound;
    }
}