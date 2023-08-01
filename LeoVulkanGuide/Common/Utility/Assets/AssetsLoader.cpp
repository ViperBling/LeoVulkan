#include "AssetsLoader.hpp"

namespace Assets
{
    bool SaveBinaryFile(const std::string& filename, const AssetFile& file)
    {
        std::ofstream ofs;
        ofs.open(filename.c_str(), std::ios::binary | std::ios::out);
        if (!ofs.is_open())
        {
            std::cout << "Error when trying to write file: " << filename << std::endl;
        }
        ofs.write(file.mType, 4);
        uint32_t version = file.mVersion;
        ofs.write((const char*)&version, sizeof(uint32_t));

        // js length
        auto length = static_cast<uint32_t>(file.mJs.size());
        ofs.write((const char*)&length, sizeof(uint32_t));

        // blob length
        auto blobLen = static_cast<uint32_t>(file.mBinaryBlob.size());
        ofs.write((const char*)&blobLen, sizeof(uint32_t));

        // js stream
        ofs.write(file.mJs.data(), length);
        // pixel data
        ofs.write(file.mBinaryBlob.data(), file.mBinaryBlob.size());

        ofs.close();

        return true;
    }

    bool LoadBinaryFile(const std::string& path, AssetFile& outFile)
    {
        std::ifstream ifs;
        ifs.open(path, std::ios::binary);

        if (!ifs.is_open()) return false;

        ifs.seekg(0);

        ifs.read(outFile.mType, 4);

        ifs.read((char*)&outFile.mVersion, sizeof(uint32_t));

        uint32_t jsonLen = 0;
        ifs.read((char*)&jsonLen, sizeof(uint32_t));

        uint32_t blobLen = 0;
        ifs.read((char*)&blobLen, sizeof(uint32_t));

        outFile.mJs.resize(jsonLen);

        ifs.read(outFile.mJs.data(), jsonLen);

        outFile.mBinaryBlob.resize(blobLen);
        ifs.read(outFile.mBinaryBlob.data(), blobLen);

        return true;
    }

    Assets::CompressionMode ParseCompression(const std::string & f)
    {
        if (strcmp(f.c_str(), "LZ4") == 0)
        {
            return Assets::CompressionMode::LZ4;
        }
        else
        {
            return Assets::CompressionMode::None;
        }
    }
}