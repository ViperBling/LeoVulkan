#include "json.hpp"
#include "lz4.h"

#include "MeshAsset.hpp"

Assets::VertexFormat ParseFormat(const char* f) {

    if (strcmp(f, "PNCVF32") == 0)
    {
        return Assets::VertexFormat::PNCVF32;
    }
    else if (strcmp(f, "P32N8C8V16") == 0)
    {
        return Assets::VertexFormat::P32N8C8V16;
    }
    else
    {
        return Assets::VertexFormat::Unknown;
    }
}

namespace Assets
{
    MeshInfo ReadMeshInfo(AssetFile *file)
    {
        MeshInfo info;

        nlohmann::json metaData = nlohmann::json::parse(file->mJs);

        info.mVBSize = metaData["VertexBufferSize"];
        info.mIBSize = metaData["IndexBufferSize"];
        info.mIndexSize = (uint8_t)metaData["IndexSize"];
        info.mOriginalFile = metaData["OriginalFile"];

        std::string compressionString = metaData["Compression"];
        info.mCompressionMode = ParseCompression(compressionString.c_str());

        std::vector<float> boundsData;
        boundsData.reserve(7);
        boundsData = metaData["Bounds"].get<std::vector<float>>();

        info.mBounds.mOrigin[0] = boundsData[0];
        info.mBounds.mOrigin[1] = boundsData[1];
        info.mBounds.mOrigin[2] = boundsData[2];

        info.mBounds.mRadius = boundsData[3];

        info.mBounds.mExtents[0] = boundsData[4];
        info.mBounds.mExtents[1] = boundsData[5];
        info.mBounds.mExtents[2] = boundsData[6];

        std::string vertexFormat = metaData["VertexFormat"];
        info.mVertexFormat = ParseFormat(vertexFormat.c_str());
        return info;
    }

    void UnpackMesh(MeshInfo *info, const char *srcBuffer, size_t srcSize, char *vertexBuffer, char *indexBuffer)
    {
        //decompressing into temporal vector. TODO: streaming decompress directly on the buffers
        std::vector<char> decompressedBuffer;
        decompressedBuffer.resize(info->mVBSize + info->mIBSize);

        LZ4_decompress_safe(srcBuffer, decompressedBuffer.data(), static_cast<int>(srcSize), static_cast<int>(decompressedBuffer.size()));

        //copy vertex buffer
        memcpy(vertexBuffer, decompressedBuffer.data(), info->mVBSize);
        //copy index buffer
        memcpy(indexBuffer, decompressedBuffer.data() + info->mVBSize, info->mIBSize);
    }

    AssetFile PackMesh(MeshInfo *info, char *vertexData, char *indexData)
    {
        AssetFile file;
        file.mType[0] = 'M';
        file.mType[1] = 'E';
        file.mType[2] = 'S';
        file.mType[3] = 'H';
        file.mVersion = 1;

        nlohmann::json metadata;
        if (info->mVertexFormat == VertexFormat::P32N8C8V16) {
            metadata["VertexFormat"] = "P32N8C8V16";
        }
        else if (info->mVertexFormat == VertexFormat::PNCVF32)
        {
            metadata["VertexFormat"] = "PNCVF32";
        }
        metadata["VertexBufferSize"] = info->mVBSize;
        metadata["IndexBufferSize"] = info->mIBSize;
        metadata["IndexSize"] = info->mIndexSize;
        metadata["OriginalFile"] = info->mOriginalFile;

        std::vector<float> boundsData;
        boundsData.resize(7);

        boundsData[0] = info->mBounds.mOrigin[0];
        boundsData[1] = info->mBounds.mOrigin[1];
        boundsData[2] = info->mBounds.mOrigin[2];

        boundsData[3] = info->mBounds.mRadius;

        boundsData[4] = info->mBounds.mExtents[0];
        boundsData[5] = info->mBounds.mExtents[1];
        boundsData[6] = info->mBounds.mExtents[2];

        metadata["Bounds"] = boundsData;

        size_t fullSize = info->mVBSize + info->mIBSize;

        std::vector<char> mergedBuffer;
        mergedBuffer.resize(fullSize);

        //copy vertex buffer
        memcpy(mergedBuffer.data(), vertexData, info->mVBSize);

        //copy index buffer
        memcpy(mergedBuffer.data() + info->mVBSize, indexData, info->mIBSize);

        //compress buffer and copy it into the file struct
        size_t compressStaging = LZ4_compressBound(static_cast<int>(fullSize));

        file.mBinaryBlob.resize(compressStaging);

        int compressedSize = LZ4_compress_default(mergedBuffer.data(), file.mBinaryBlob.data(), static_cast<int>(mergedBuffer.size()), static_cast<int>(compressStaging));
        file.mBinaryBlob.resize(compressedSize);

        metadata["Compression"] = "LZ4";

        file.mJs = metadata.dump();

        return file;
    }

    MeshBounds CalculateBounds(VertexF32PNCV *vertices, size_t count)
    {
        MeshBounds bounds{};

        float min[3] = { std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max() };
        float max[3] = { std::numeric_limits<float>::min(),std::numeric_limits<float>::min(),std::numeric_limits<float>::min() };

        for (int i = 0; i < count; i++)
        {
            min[0] = std::min(min[0], vertices[i].mPosition[0]);
            min[1] = std::min(min[1], vertices[i].mPosition[1]);
            min[2] = std::min(min[2], vertices[i].mPosition[2]);

            max[0] = std::max(max[0], vertices[i].mPosition[0]);
            max[1] = std::max(max[1], vertices[i].mPosition[1]);
            max[2] = std::max(max[2], vertices[i].mPosition[2]);
        }

        bounds.mExtents[0] = (max[0] - min[0]) / 2.0f;
        bounds.mExtents[1] = (max[1] - min[1]) / 2.0f;
        bounds.mExtents[2] = (max[2] - min[2]) / 2.0f;

        bounds.mOrigin[0] = bounds.mExtents[0] + min[0];
        bounds.mOrigin[1] = bounds.mExtents[1] + min[1];
        bounds.mOrigin[2] = bounds.mExtents[2] + min[2];

        //go through the vertices again to calculate the exact bounding sphere radius
        float r2 = 0;
        for (int i = 0; i < count; i++)
        {
            float offset[3];
            offset[0] = vertices[i].mPosition[0] - bounds.mOrigin[0];
            offset[1] = vertices[i].mPosition[1] - bounds.mOrigin[1];
            offset[2] = vertices[i].mPosition[2] - bounds.mOrigin[2];

            float distance = offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2];
            r2 = std::max(r2, distance);
        }
        bounds.mRadius = std::sqrt(r2);

        return bounds;
    }
}