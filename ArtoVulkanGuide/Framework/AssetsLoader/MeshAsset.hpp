#pragma once

#include "AssetsLoader.hpp"

namespace Assets
{
    struct VertexF32PNCV
    {
        float mPosition[3];
        float mNormal[3];
        float mColor[3];
        float mUV[2];
    };
    struct VertexP32N8C8V16
    {
        float mPosition[3];
        uint8_t mNormal[3];
        uint8_t mColor[3];
        float mUV[2];
    };

    enum class VertexFormat : uint32_t
    {
        Unknown = 0,
        PNCVF32,    //everything at 32 bits
        P32N8C8V16  //position at 32 bits, normal at 8 bits, color at 8 bits, uvs at 16 bits float
    };

    struct MeshBounds
    {
        float mOrigin[3];
        float mRadius;
        float mExtents[3];
    };

    struct MeshInfo
    {
        uint64_t mVBSize;
        uint64_t mIBSize;
        MeshBounds mBounds;
        VertexFormat mVertexFormat;
        char mIndexSize;
        CompressionMode mCompressionMode;
        std::string mOriginalFile;
    };

    MeshInfo ReadMeshInfo(AssetFile* file);
    void UnpackMesh(MeshInfo* info, const char* srcBuffer, size_t srcSize, char* vertexBuffer, char* indexBuffer);
    AssetFile PackMesh(MeshInfo* info, char* vertexData, char* indexData);
    MeshBounds CalculateBounds(VertexF32PNCV* vertices, size_t count);
}