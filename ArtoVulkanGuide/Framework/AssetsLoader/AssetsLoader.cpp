#include <fstream>
#include <iostream>

#include "AssetsLoader.hpp"

namespace Assets
{
    bool SaveBinaryFile(const char *path, const AssetFile& file)
    {
        std::ofstream outFile;
        outFile.open(path, std::ios::binary | std::ios::out);
        if (!outFile.is_open())
        {
            std::cout << "Error when trying to write file: " << path << std::endl;
            return false;
        }
        outFile.write(file.mType, 4);
        uint32_t version = file.mVersion;

        //version
        outFile.write((const char*)&version, sizeof(uint32_t));

        //json length
        auto length = static_cast<uint32_t>(file.mJs.size());
        outFile.write((const char*)&length, sizeof(uint32_t));

        //blob length
        auto blobLen = static_cast<uint32_t>(file.mBinaryBlob.size());
        outFile.write((const char*)&blobLen, sizeof(uint32_t));

        //json stream
        outFile.write(file.mJs.data(), length);
        //pixel data
        outFile.write(file.mBinaryBlob.data(), file.mBinaryBlob.size());

        outFile.close();

        return true;
    }

    bool LoadBinaryFile(const char *path, AssetFile& outFile)
    {
        std::ifstream inFile;
        inFile.open(path, std::ios::binary);

        if (!inFile.is_open()) return false;

        inFile.seekg(0);

        inFile.read(outFile.mType, 4);

        inFile.read((char*)&outFile.mVersion, sizeof(uint32_t));

        uint32_t jsonLen = 0;
        inFile.read((char*)&jsonLen, sizeof(uint32_t));

        uint32_t blobLen = 0;
        inFile.read((char*)&blobLen, sizeof(uint32_t));

        outFile.mJs.resize(jsonLen);

        inFile.read(outFile.mJs.data(), jsonLen);

        outFile.mBinaryBlob.resize(blobLen);
        inFile.read(outFile.mBinaryBlob.data(), blobLen);

        return true;
    }

    Assets::CompressionMode ParseCompression(const char *file)
    {
        if (strcmp(file, "LZ4") == 0)
        {
            return Assets::CompressionMode::LZ4;
        }
        else {
            return Assets::CompressionMode::None;
        }
    }
}