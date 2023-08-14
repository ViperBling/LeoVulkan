#pragma once

#include <vector>
#include <string>

namespace Assets
{
    struct AssetFile
    {
        char mType[4];
        int mVersion;
        std::string mJs;
        std::vector<char> mBinaryBlob;
    };
    enum class CompressionMode : uint32_t
    {
        None, LZ4
    };

    bool SaveBinaryFile(const char* path, const AssetFile& file);
    bool LoadBinaryFile(const char* path, AssetFile& outFile);
    Assets::CompressionMode ParseCompression(const char* file);
}