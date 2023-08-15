#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>

#include "json.hpp"
#include "lz4.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "tiny_obj_loader.h"

#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#include "nvtt.h"
#include "glm.hpp"
#include "gtx/transform.hpp"
#include "gtx/quaternion.hpp"

#include "AssetsLoader.hpp"
#include "TextureAsset.hpp"
#include "MeshAsset.hpp"
#include "MaterialAsset.hpp"
#include "PrefabAsset.hpp"

namespace fs = std::filesystem;

using namespace Assets;

struct ConverterState
{
    [[nodiscard]] fs::path ConvertToExportRelative(fs::path path) const;

    fs::path mAssetPath;
    fs::path mExportPath;
};

fs::path ConverterState::ConvertToExportRelative(fs::path path) const
{
    return path.lexically_proximate(mExportPath);
}

bool ConvertImage(const fs::path& input, const fs::path& output);

void PackVertex(
    Assets::VertexF32PNCV& newVert,
    tinyobj::real_t vx, tinyobj::real_t vy, tinyobj::real_t vz,
    tinyobj::real_t nx, tinyobj::real_t ny, tinyobj::real_t nz,
    tinyobj::real_t ux, tinyobj::real_t uy);
void PackVertex(
    Assets::VertexP32N8C8V16& newVert,
    tinyobj::real_t vx, tinyobj::real_t vy, tinyobj::real_t vz,
    tinyobj::real_t nx, tinyobj::real_t ny, tinyobj::real_t nz,
    tinyobj::real_t ux, tinyobj::real_t uy);

template<typename V>
void ExtractMeshFromObj(
    std::vector<tinyobj::shape_t>& shapes,
    tinyobj::attrib_t& attrib, std::vector<uint32_t>& indices, std::vector<V>& vertices);

bool ConvertMesh(const fs::path& input, const fs::path& output);
void UnpackGLTFBuffer(tinygltf::Model& model, tinygltf::Accessor& accessor, std::vector<uint8_t>& outBuffer);
void ExtractVertices(tinygltf::Primitive& primitive, tinygltf::Model& model, std::vector<Assets::VertexF32PNCV>& vertices);
void ExtractIndices(tinygltf::Primitive& primitive, tinygltf::Model& model, std::vector<uint32_t>& indices);

std::string GetGLTFMeshName(tinygltf::Model& model, int meshIndex, int primitiveIndex);
bool ExtractGLTFMesh(tinygltf::Model& model, const fs::path& input, const fs::path& outputFolder, const ConverterState& convState);

std::string GetGLTFMaterialName(tinygltf::Model& model, int materialIndex);
void ExtractGLTFMaterials(tinygltf::Model& model, const fs::path& input, const fs::path& outputFolder, const ConverterState& convState);

void ExtractGLTFNodes(tinygltf::Model& model, const fs::path& input, const fs::path& outputFolder, const ConverterState& convState);

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Need to put the path to the info file";
        return -1;
    }
    else
    {
        fs::path path{argv[1]};
        fs::path directory = path;
        fs::path exportedDir = path.parent_path() / "AssetsExport";
        std::cout << "Loaded Asset Directory at " << directory << std::endl;

        ConverterState converterState;
        converterState.mAssetPath = path;
        converterState.mExportPath = exportedDir;

        for (auto& p : fs::recursive_directory_iterator(directory))
        {
            std::cout << "File: " << p << std::endl;

            auto relative = p.path().lexically_proximate(directory);
            auto exportPath = exportedDir / relative;

            if (!fs::is_directory(exportPath.parent_path())) fs::create_directory(exportPath.parent_path());

            if (p.path().extension() == ".gltf")
            {
                tinygltf::Model model;
                tinygltf::TinyGLTF loader;
                std::string err;
                std::string warn;

                bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, p.path().string().c_str());

                if (!warn.empty()) std::cout << "Warning: " << warn << std::endl;
                if (!err.empty()) std::cout << "Error: " << err << std::endl;

                if (!ret)
                {
                    std::cout << "Failed to parse GLTF" << std::endl;
                    return -1;
                }
                else
                {
                    auto folder = exportPath.parent_path() / (p.path().stem().string() + "_GLTF");
                    fs::create_directory(folder);

                    ExtractGLTFMesh(model, p.path(), folder, converterState);
                    ExtractGLTFMaterials(model, p.path(), folder, converterState);
                    ExtractGLTFNodes(model, p.path(), folder, converterState);
                }
            }
        }
    }
    return 0;
}

bool ConvertImage(const fs::path &input, const fs::path &output)
{
    int texW, texH, texC;
    auto pngStart = std::chrono::high_resolution_clock::now();

    stbi_uc* pixels = stbi_load(input.u8string().c_str(), &texW, &texH, &texC, STBI_rgb_alpha);

    auto pngEnd = std::chrono::high_resolution_clock::now();
    auto diff = pngEnd - pngStart;

    std::cout << "PNG Took: " << (double)std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count() / 1000000.0 << "ms" << std::endl;

    if (!pixels)
    {
        std::cout << "Failed to load texture file: " << input << std::endl;
        return false;
    }

    int texSize = texW * texH * 4;

    TextureInfo texInfo;
    texInfo.mTexSize = texSize;
    texInfo.mTexFormat = TextureFormat::RGBA8;
    texInfo.mOriginalFile = input.string();

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<char> allBuffer;

    struct DummyHandler : nvtt::OutputHandler
    {
        std::vector<char> mBuffer;

        virtual bool writeData(const void * data, int size)
        {
            for (int i = 0; i < size; i++) mBuffer.push_back(((char*)data)[i]);
            return true;
        }
        virtual void beginImage(int size, int width, int height, int depth, int face, int mipLevel) { };
        // Indicate the end of the compressed image. (New in NVTT 2.1)
        virtual void endImage() {};
    };

    nvtt::Compressor compressor;
    nvtt::CompressionOptions compOptions;
    nvtt::OutputOptions outputOptions;
    nvtt::Surface surface;

    DummyHandler handler;
    outputOptions.setOutputHandler(&handler);
    surface.setImage(nvtt::InputFormat::InputFormat_BGRA_8UB, texW, texH, 1, pixels);

    while (surface.canMakeNextMipmap(1))
    {
        surface.buildNextMipmap(nvtt::MipmapFilter_Box);
        compOptions.setFormat(nvtt::Format::Format_RGBA);
        compOptions.setPixelType(nvtt::PixelType_UnsignedNorm);

        compressor.compress(surface, 0, 0, compOptions, outputOptions);

        texInfo.mPages.push_back({});
        texInfo.mPages.back().mWidth = surface.width();
        texInfo.mPages.back().mHeight = surface.height();
        texInfo.mPages.back().mOriginalSize = handler.mBuffer.size();

        allBuffer.insert(allBuffer.end(), handler.mBuffer.begin(), handler.mBuffer.end());
        handler.mBuffer.clear();
    }

    texInfo.mTexSize = allBuffer.size();
    Assets::AssetFile newImage = Assets::PackTexture(&texInfo, allBuffer.data());

    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "Compression took: " << (double)std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count() / 1000000.0 << "ms" << std::endl;

    stbi_image_free(pixels);

    SaveBinaryFile(output.string().c_str(), newImage);
    return true;
}

void PackVertex(
    VertexF32PNCV &newVert,
    tinyobj::real_t vx, tinyobj::real_t vy, tinyobj::real_t vz,
    tinyobj::real_t nx, tinyobj::real_t ny, tinyobj::real_t nz,
    tinyobj::real_t ux, tinyobj::real_t uy)
{
    newVert.mPosition[0] = vx;
    newVert.mPosition[1] = vy;
    newVert.mPosition[2] = vz;

    newVert.mNormal[0] = nx;
    newVert.mNormal[1] = ny;
    newVert.mNormal[2] = nz;

    newVert.mUV[0] = ux;
    newVert.mUV[1] = 1 - uy;
}

void PackVertex(
    VertexP32N8C8V16 &newVert,
    tinyobj::real_t vx, tinyobj::real_t vy, tinyobj::real_t vz,
    tinyobj::real_t nx, tinyobj::real_t ny, tinyobj::real_t nz,
    tinyobj::real_t ux, tinyobj::real_t uy)
{
    newVert.mPosition[0] = vx;
    newVert.mPosition[1] = vy;
    newVert.mPosition[2] = vz;

    newVert.mNormal[0] = uint8_t(  ((nx + 1.0) / 2.0) * 255);
    newVert.mNormal[1] = uint8_t(  ((ny + 1.0) / 2.0) * 255);
    newVert.mNormal[2] = uint8_t(  ((nz + 1.0) / 2.0) * 255);

    newVert.mUV[0] = ux;
    newVert.mUV[1] = 1 - uy;
}

template<typename V>
void ExtractMeshFromObj(
    std::vector<tinyobj::shape_t> &shapes,
    tinyobj::attrib_t &attrib, std::vector<uint32_t> &indices,
    std::vector<V> &vertices)
{
    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++)
    {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            //hardcode loading to triangles
            int fv = 3;

            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++)
            {
                // access to assets::Vertex_f32_PNCV
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                //vertex position
                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
                //vertex normal
                tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

                //vertex uv
                tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
                tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];

                //copy it into our vertex
                V newVert;
                PackVertex(newVert, vx, vy, vz, nx, ny, nz, ux, uy);

                indices.push_back(vertices.size());
                vertices.push_back(newVert);
            }
            index_offset += fv;
        }
    }
}

bool ConvertMesh(const fs::path &input, const fs::path &output)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    auto objStart = std::chrono::high_resolution_clock::now();
    //load the OBJ file
    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, input.string().c_str(), nullptr);
    auto objEnd = std::chrono::high_resolution_clock::now();

    auto diff = objEnd - objStart;

    std::cout << "Obj took " << (double)std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count() / 1000000.0 << "ms" << std::endl;

    //make sure to output the warnings to the console, in case there are issues with the file
    if (!warn.empty()) std::cout << "WARN: " << warn << std::endl;

    if (!err.empty())
    {
        std::cerr << err << std::endl;
        return false;
    }

    using VertexFormat = Assets::VertexF32PNCV;
    auto vertexFormatEnum = Assets::VertexFormat::PNCVF32;

    std::vector<VertexFormat> vertices;
    std::vector<uint32_t> indices;

    ExtractMeshFromObj(shapes, attrib, indices, vertices);

    MeshInfo meshInfo;
    meshInfo.mVertexFormat = vertexFormatEnum;
    meshInfo.mVBSize = vertices.size() * sizeof(VertexFormat);
    meshInfo.mIBSize = indices.size() * sizeof(uint32_t);
    meshInfo.mIndexSize = sizeof(uint32_t);
    meshInfo.mOriginalFile = input.string();
    meshInfo.mBounds = Assets::CalculateBounds(vertices.data(), vertices.size());

    auto start = std::chrono::high_resolution_clock::now();
    Assets::AssetFile newFile = Assets::PackMesh(&meshInfo, (char*)vertices.data(), (char*)indices.data());
    auto  end = std::chrono::high_resolution_clock::now();

    diff = end - start;
    std::cout << "compression took " << (double)std::chrono::duration_cast<std::chrono::nanoseconds>(diff).count() / 1000000.0 << "ms" << std::endl;

    SaveBinaryFile(output.string().c_str(), newFile);

    return true;
}

void UnpackGLTFBuffer(tinygltf::Model &model, tinygltf::Accessor &accessor, std::vector<uint8_t> &outBuffer)
{
    int bufferID = accessor.bufferView;
    size_t elementSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);

    tinygltf::BufferView& bufferView = model.bufferViews[bufferID];
    tinygltf::Buffer& bufferData = (model.buffers[bufferView.buffer]);

    uint8_t* dataPtr = bufferData.data.data() + accessor.byteOffset + bufferView.byteOffset;

    int components = tinygltf::GetNumComponentsInType(accessor.type);

    elementSize *= components;

    size_t stride = bufferView.byteStride;
    if (stride == 0)
    {
        stride = elementSize;

    }
    outBuffer.resize(accessor.count * elementSize);

    for (int i = 0; i < accessor.count; i++)
    {
        uint8_t* dataIndex = dataPtr + stride * i;
        uint8_t* targetPtr = outBuffer.data() + elementSize * i;
        memcpy(targetPtr, dataIndex, elementSize);
    }
}

void ExtractVertices(tinygltf::Primitive &primitive, tinygltf::Model &model, std::vector<Assets::VertexF32PNCV> &vertices)
{
    tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes["POSITION"]];
    vertices.resize(posAccessor.count);
    std::vector<uint8_t> posData;
    UnpackGLTFBuffer(model, posAccessor, posData);

    for (int i = 0; i < vertices.size(); i++)
    {
        if (posAccessor.type == TINYGLTF_TYPE_VEC3)
        {
            if (posAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
            {
                auto dtf = (float*)posData.data();

                //vec3f
                vertices[i].mPosition[0] = *(dtf + (i * 3) + 0);
                vertices[i].mPosition[1] = *(dtf + (i * 3) + 1);
                vertices[i].mPosition[2] = *(dtf + (i * 3) + 2);
            }
            else
            {
                assert(false);
            }
        }
        else
        {
            assert(false);
        }
    }

    tinygltf::Accessor& normalAccessor = model.accessors[primitive.attributes["NORMAL"]];
    std::vector<uint8_t> normalData;
    UnpackGLTFBuffer(model, normalAccessor, normalData);

    for (int i = 0; i < vertices.size(); i++)
    {
        if (normalAccessor.type == TINYGLTF_TYPE_VEC3)
        {
            if (normalAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
            {
                auto dtf = (float*)normalData.data();

                //vec3f
                vertices[i].mNormal[0] = *(dtf + (i * 3) + 0);
                vertices[i].mNormal[1] = *(dtf + (i * 3) + 1);
                vertices[i].mNormal[2] = *(dtf + (i * 3) + 2);

                vertices[i].mColor[0] = *(dtf + (i * 3) + 0);
                vertices[i].mColor[1] = *(dtf + (i * 3) + 1);
                vertices[i].mColor[2] = *(dtf + (i * 3) + 2);
            }
            else
            {
                assert(false);
            }
        }
        else
        {
            assert(false);
        }
    }

    tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes["TEXCOORD_0"]];
    std::vector<uint8_t> uvData;
    UnpackGLTFBuffer(model, uvAccessor, uvData);

    for (int i = 0; i < vertices.size(); i++)
    {
        if (uvAccessor.type == TINYGLTF_TYPE_VEC2)
        {
            if (uvAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
            {
                auto dtf = (float*)uvData.data();

                //vec3f
                vertices[i].mUV[0] = *(dtf + (i * 2) + 0);
                vertices[i].mUV[1] = *(dtf + (i * 2) + 1);
            }
            else
            {
                assert(false);
            }
        }
        else
        {
            assert(false);
        }
    }
}

void ExtractIndices(tinygltf::Primitive &primitive, tinygltf::Model &model, std::vector<uint32_t> &indices)
{
    int indexAccessor = primitive.indices;

    int indexBuffer = model.accessors[indexAccessor].bufferView;
    int componentType = model.accessors[indexAccessor].componentType;
    size_t indexSize = tinygltf::GetComponentSizeInBytes(componentType);

    tinygltf::BufferView& indexView = model.bufferViews[indexBuffer];
    int bufferIdx = indexView.buffer;

    tinygltf::Buffer& bufferIndex = (model.buffers[bufferIdx]);

    uint8_t* dataPtr = bufferIndex.data.data() + indexView.byteOffset;

    std::vector<uint8_t> unpackedIndices;
    UnpackGLTFBuffer(model, model.accessors[indexAccessor], unpackedIndices);

    for (int i = 0; i < model.accessors[indexAccessor].count; i++)
    {
        uint32_t index;
        switch (componentType)
        {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            {
                auto bfr = (uint16_t*)unpackedIndices.data();
                index = *(bfr + i);
            }
                break;
            case TINYGLTF_COMPONENT_TYPE_SHORT:
            {
                auto bfr = (int16_t*)unpackedIndices.data();
                index = *(bfr + i);
            }
                break;
            default:
                assert(false);
        }
        indices.push_back(index);
    }

    for (int i = 0; i < indices.size() / 3; i++)
    {
        //flip the triangle
        std::swap(indices[i * 3 + 1], indices[i * 3 + 2]);
    }
}

std::string GetGLTFMeshName(tinygltf::Model &model, int meshIndex, int primitiveIndex)
{
    char buffer0[50];
    char buffer1[50];
    _itoa_s(meshIndex, buffer0, 10);
    _itoa_s(primitiveIndex, buffer1, 10);

    std::string meshName = "MESH_" + std::string{ &buffer0[0] } + "_" + model.meshes[meshIndex].name;

    bool multiPrimitive = model.meshes[meshIndex].primitives.size() > 1;
    if (multiPrimitive)
    {
        meshName += "_PRIM_" + std::string{ &buffer1[0] };
    }

    return meshName;
}

bool ExtractGLTFMesh(
    tinygltf::Model &model,
    const fs::path &input, const fs::path &outputFolder,
    const ConverterState &convState)
{
    tinygltf::Model* gltfModel = &model;
    for (auto meshIndex = 0; meshIndex < model.meshes.size(); meshIndex++)
    {
        auto& gltfMesh = model.meshes[meshIndex];

        using VertexFormat = Assets::VertexF32PNCV;
        auto VertexFormatEnum = Assets::VertexFormat::PNCVF32;

        std::vector<VertexFormat> vertices;
        std::vector<uint32_t> indices;

        for (auto primitiveIndex = 0; primitiveIndex < gltfMesh.primitives.size(); primitiveIndex++)
        {
            vertices.clear();
            indices.clear();

            std::string meshName = GetGLTFMeshName(model, meshIndex, primitiveIndex);

            auto& primitive = gltfMesh.primitives[primitiveIndex];

            ExtractIndices(primitive, model, indices);
            ExtractVertices(primitive, model, vertices);

            MeshInfo meshInfo;
            meshInfo.mVertexFormat = VertexFormatEnum;
            meshInfo.mVBSize = vertices.size() * sizeof(VertexFormat);
            meshInfo.mIBSize = indices.size() * sizeof(uint32_t);
            meshInfo.mIndexSize = sizeof(uint32_t);
            meshInfo.mOriginalFile = input.string();
            meshInfo.mBounds = Assets::CalculateBounds(vertices.data(), vertices.size());

            Assets::AssetFile newFile = Assets::PackMesh(&meshInfo, (char*)vertices.data(), (char*)indices.data());

            fs::path meshPath = outputFolder / (meshName + ".mesh");

            SaveBinaryFile(meshPath.string().c_str(), newFile);
        }
    }
    return true;
}

std::string GetGLTFMaterialName(tinygltf::Model &model, int materialIndex)
{
    char buffer[50];

    _itoa_s(materialIndex, buffer, 10);
    std::string matName = "MAT_" + std::string{ &buffer[0] } + "_" + model.materials[materialIndex].name;
    return matName;
}

void ExtractGLTFMaterials(
    tinygltf::Model &model,
    const fs::path &input, const fs::path &outputFolder,
    const ConverterState &convState)
{
    int numMat = 0;
    for (auto& gltfMat : model.materials)
    {
        std::string matName = GetGLTFMaterialName(model, numMat);

        numMat++;
        auto& pbr = gltfMat.pbrMetallicRoughness;

        Assets::MaterialInfo newMaterial;
        newMaterial.mBaseEffect = "DefaultPBR";
        {
            if (pbr.baseColorTexture.index < 0)
            {
                pbr.baseColorTexture.index = 0;
            }
            auto baseColor = model.textures[pbr.baseColorTexture.index];
            auto baseImage = model.images[baseColor.source];

            fs::path baseColorPath = outputFolder.parent_path() / baseImage.uri;

            baseColorPath.replace_extension(".tx");

            baseColorPath = convState.ConvertToExportRelative(baseColorPath);

            newMaterial.mTextures["BaseColor"] = baseColorPath.string();
        }
        if (pbr.metallicRoughnessTexture.index >= 0)
        {
            auto image = model.textures[pbr.metallicRoughnessTexture.index];
            auto baseImage = model.images[image.source];

            fs::path baseColorPath = outputFolder.parent_path() / baseImage.uri;

            baseColorPath.replace_extension(".tx");

            baseColorPath = convState.ConvertToExportRelative(baseColorPath);

            newMaterial.mTextures["MetallicRoughness"] = baseColorPath.string();
        }

        if (gltfMat.normalTexture.index >= 0)
        {
            auto image = model.textures[gltfMat.normalTexture.index];
            auto baseImage = model.images[image.source];

            fs::path baseColorPath = outputFolder.parent_path() / baseImage.uri;

            baseColorPath.replace_extension(".tx");

            baseColorPath = convState.ConvertToExportRelative(baseColorPath);

            newMaterial.mTextures["Normals"] = baseColorPath.string();
        }

        if (gltfMat.occlusionTexture.index >= 0)
        {
            auto image = model.textures[gltfMat.occlusionTexture.index];
            auto baseImage = model.images[image.source];

            fs::path baseColorPath = outputFolder.parent_path() / baseImage.uri;

            baseColorPath.replace_extension(".tx");

            baseColorPath = convState.ConvertToExportRelative(baseColorPath);

            newMaterial.mTextures["Occlusion"] = baseColorPath.string();
        }

        if (gltfMat.emissiveTexture.index >= 0)
        {
            auto image = model.textures[gltfMat.emissiveTexture.index];
            auto baseImage = model.images[image.source];

            fs::path baseColorPath = outputFolder.parent_path() / baseImage.uri;

            baseColorPath.replace_extension(".tx");

            baseColorPath = convState.ConvertToExportRelative(baseColorPath);

            newMaterial.mTextures["Emissive"] = baseColorPath.string();
        }

        fs::path materialPath = outputFolder / (matName + ".mat");

        if (gltfMat.alphaMode == "BLEND")
        {
            newMaterial.mTransparency = TransparencyMode::Transparent;
        }
        else
        {
            newMaterial.mTransparency = TransparencyMode::Opaque;
        }

        Assets::AssetFile newFile = Assets::PackMaterial(&newMaterial);

        //save to disk
        SaveBinaryFile(materialPath.string().c_str(), newFile);
    }
}

void ExtractGLTFNodes(
    tinygltf::Model &model,
    const fs::path &input, const fs::path &outputFolder,
    const ConverterState &convState)
{
    Assets::PrefabInfo prefab;

    std::vector<uint64_t> meshNodes;
    for (int i = 0; i < model.nodes.size(); i++)
    {
        auto& node = model.nodes[i];

        std::string nodeName = node.name;
        prefab.mNodeNames[i] = nodeName;

        std::array<float, 16> matrix{};

        //node has a matrix
        if (!node.matrix.empty())
        {
            for (int n = 0; n < 16; n++)
            {
                matrix[n] = node.matrix[n];
            }
            glm::mat4 mat;
            memcpy(&mat, &matrix, sizeof(glm::mat4));

            mat = mat;  // * flip;
            memcpy(matrix.data(), &mat, sizeof(glm::mat4));
        }
        else
        {
            //separate transform
            glm::mat4 translation{ 1.f };
            if (!node.translation.empty())
            {
                translation = glm::translate(glm::vec3{ node.translation[0],node.translation[1] ,node.translation[2] });
            }

            glm::mat4 rotation{ 1.f };

            if (!node.rotation.empty())
            {
                glm::quat rot( node.rotation[3],  node.rotation[0],node.rotation[1],node.rotation[2]);
                rotation = glm::mat4{rot};
            }

            glm::mat4 scale{ 1.f };
            if (!node.scale.empty())
            {
                scale = glm::scale(glm::vec3{ node.scale[0],node.scale[1] ,node.scale[2] });
            }

            glm::mat4 transformMatrix = (translation * rotation * scale);// * flip;

            memcpy(matrix.data(), &transformMatrix, sizeof(glm::mat4));
        }

        prefab.mNodeMatrices[i] = prefab.mMatrices.size();
        prefab.mMatrices.push_back(matrix);

        if (node.mesh >= 0)
        {
            auto mesh = model.meshes[node.mesh];

            if (mesh.primitives.size() > 1)
            {
                meshNodes.push_back(i);
            }
            else
            {
                auto primitive = mesh.primitives[0];
                std::string meshName = GetGLTFMeshName(model, node.mesh, 0);

                fs::path meshPath = outputFolder / (meshName + ".mesh");

                int material = primitive.material;

                std::string matName = GetGLTFMaterialName(model, material);

                fs::path materialPath = outputFolder / (matName + ".mat");

                Assets::PrefabInfo::NodeMesh nodeMesh;
                nodeMesh.mMeshPath = convState.ConvertToExportRelative(meshPath).string();
                nodeMesh.mMaterialPath = convState.ConvertToExportRelative(materialPath).string();

                prefab.mNodeMeshes[i] = nodeMesh;
            }
        }
    }
    //calculate parent hierarchies
    //gltf stores children, but we want parent
    for (int i = 0; i < model.nodes.size(); i++)
    {
        for (auto c : model.nodes[i].children)
        {
            prefab.mNodeParents[c] = i;
        }
    }

    //for every gltf node that is a root node (no parents), apply the coordinate fixup
    glm::mat4 flip = glm::mat4{ 1.0 };
    flip[1][1] = -1;

    glm::mat4 rotation = glm::mat4{ 1.0 };
    //flip[1][1] = -1;
    rotation = glm::rotate(glm::radians(-180.f), glm::vec3{ 1,0,0 });

    //flip[2][2] = -1;
    for (int i = 0; i < model.nodes.size(); i++)
    {
        auto it = prefab.mNodeParents.find(i);
        if (it == prefab.mNodeParents.end())
        {
            auto matrix = prefab.mMatrices[prefab.mNodeMatrices[i]];
            //no parent, root node
            glm::mat4 mat;
            memcpy(&mat, &matrix, sizeof(glm::mat4));

            mat =rotation*(flip* mat);
            memcpy(&matrix, &mat, sizeof(glm::mat4));

            prefab.mMatrices[prefab.mNodeMatrices[i]] = matrix;
        }
    }

    int nodeIndex = model.nodes.size();
    //iterate nodes with mesh, convert each submesh into a node
    for (int i = 0; i < meshNodes.size(); i++)
    {
        auto& node = model.nodes[i];

        if (node.mesh < 0) break;

        auto mesh = model.meshes[node.mesh];

        for (int primitiveIndex = 0 ; primitiveIndex < mesh.primitives.size(); primitiveIndex++)
        {
            auto primitive = mesh.primitives[primitiveIndex];
            int newNode = nodeIndex++;

            char buffer[50];

            _itoa_s(primitiveIndex, buffer, 10);

            prefab.mNodeNames[newNode] = prefab.mNodeNames[i] +  "_PRIM_" + &buffer[0];

            int material = primitive.material;
            auto mat = model.materials[material];
            std::string matName = GetGLTFMaterialName(model, material);
            std::string meshName = GetGLTFMeshName(model, node.mesh, primitiveIndex);

            fs::path materialPath = outputFolder / (matName + ".mat");
            fs::path meshPath = outputFolder / (meshName + ".mesh");

            Assets::PrefabInfo::NodeMesh nodeMesh;
            nodeMesh.mMeshPath = convState.ConvertToExportRelative(meshPath).string();
            nodeMesh.mMaterialPath = convState.ConvertToExportRelative(materialPath).string();

            prefab.mNodeMeshes[newNode] = nodeMesh;
        }
    }

    Assets::AssetFile newFile = Assets::PackPrefab(prefab);

    fs::path sceneFilePath = (outputFolder.parent_path()) / input.stem();

    sceneFilePath.replace_extension(".pfb");

    //save to disk
    SaveBinaryFile(sceneFilePath.string().c_str(), newFile);
}
