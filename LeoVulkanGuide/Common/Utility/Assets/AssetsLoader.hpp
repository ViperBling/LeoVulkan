#pragma once

#include <iostream>
#include <fstream>
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

    bool SaveBinaryFile(const std::string& path, const AssetFile& file);
    bool LoadBinaryFile(const std::string& path, AssetFile& outFile);

    Assets::CompressionMode ParseCompression(const std::string & f);
}