#include <iostream>
#include "json.hpp"
#include "lz4.h"

#include "TextureAsset.hpp"

Assets::TextureFormat ParseFormat(const char* f)
{
    if (strcmp(f, "RGBA8") == 0)
    {
        return Assets::TextureFormat::RGBA8;
    }
    else {
        return Assets::TextureFormat::Unknown;
    }
}

namespace Assets
{
    TextureInfo ReadTextureInfo(AssetFile *file)
    {
        TextureInfo info;

        nlohmann::json texture_metadata = nlohmann::json::parse(file->mJs);

        std::string formatString = texture_metadata["Format"];
        info.mTexFormat = ParseFormat(formatString.c_str());

        std::string compressionString = texture_metadata["Compression"];
        info.mCompressionMode = ParseCompression(compressionString.c_str());

        info.mTexSize = texture_metadata["BufferSize"];
        info.mOriginalFile = texture_metadata["OriginalFile"];

        for (auto& [key, value] : texture_metadata["Pages"].items())
        {
            PageInfo page{};

            page.mCompressedSize = value["CompressedSize"];
            page.mOriginalSize = value["OriginalSize"];
            page.mWidth = value["Width"];
            page.mHeight = value["Height"];

            info.mPages.push_back(page);
        }

        return info;
    }

    void UnpackTexture(TextureInfo *info, const char *srcBuffer, size_t srcSize, char *dst)
    {
        if (info->mCompressionMode == CompressionMode::LZ4)
        {
            for (auto& page : info->mPages)
            {
                LZ4_decompress_safe(srcBuffer, dst, page.mCompressedSize, page.mOriginalSize);
                srcBuffer += page.mCompressedSize;
                dst += page.mCompressedSize;
            }
        }
        else {
            memcpy(dst, srcBuffer, srcSize);
        }
    }

    void UnpackTexturePage(TextureInfo *info, int pageIndex, char *srcBuffer, char *dst)
    {
        char* source = srcBuffer;
        for (int i = 0; i < pageIndex; i++)
        {
            source += info->mPages[i].mCompressedSize;
        }

        if (info->mCompressionMode == CompressionMode::LZ4)
        {
            //size doesn't fully match, its compressed
            if(info->mPages[pageIndex].mCompressedSize != info->mPages[pageIndex].mCompressedSize)
            {
                LZ4_decompress_safe(source, dst, info->mPages[pageIndex].mCompressedSize, info->mPages[pageIndex].mCompressedSize);
            }
            else {
                //size matched, uncompressed page
                memcpy(dst, source, info->mPages[pageIndex].mCompressedSize);
            }
        }
        else {
            memcpy(dst, source, info->mPages[pageIndex].mCompressedSize);
        }
    }

    AssetFile PackTexture(TextureInfo *info, void *pixelData)
    {
        //core file header
        AssetFile file;
        file.mType[0] = 'T';
        file.mType[1] = 'E';
        file.mType[2] = 'X';
        file.mType[3] = 'I';
        file.mVersion = 1;

        char* pixels = (char*)pixelData;
        std::vector<char> pageBuffer;
        for (auto& p : info->mPages)
        {
            pageBuffer.resize(p.mCompressedSize);

            //compress buffer into blob
            int compressStaging = LZ4_compressBound(p.mCompressedSize);

            pageBuffer.resize(compressStaging);

            int compressedSize = LZ4_compress_default(pixels, pageBuffer.data(), p.mCompressedSize, compressStaging);

            float compression_rate = float(compressedSize) / float(info->mTexSize);

            //if the compression is more than 80% of the original size, it's not worth to use it
            if (compression_rate > 0.8)
            {
                compressedSize = p.mCompressedSize;
                pageBuffer.resize(compressedSize);

                memcpy(pageBuffer.data(), pixels, compressedSize);
            }
            else
            {
                pageBuffer.resize(compressedSize);
            }
            p.mCompressedSize = compressedSize;

            file.mBinaryBlob.insert(file.mBinaryBlob.end(), pageBuffer.begin(), pageBuffer.end());

            //advance pixel pointer to next page
            pixels += p.mCompressedSize;
        }
        nlohmann::json texture_metadata;
        texture_metadata["Format"] = "RGBA8";

        texture_metadata["BufferSize"] = info->mTexSize;
        texture_metadata["OriginalFile"] = info->mOriginalFile;
        texture_metadata["Compression"] = "LZ4";

        std::vector<nlohmann::json> pageJs;
        for (auto& p : info->mPages)
        {
            nlohmann::json page;
            page["CompressedSize"] = p.mCompressedSize;
            page["OriginalSize"] = p.mCompressedSize;
            page["Width"] = p.mWidth;
            page["Height"] = p.mHeight;
            pageJs.push_back(page);
        }
        texture_metadata["Pages"] = pageJs;

        std::string stringField = texture_metadata.dump();
        file.mJs = stringField;

        return file;
    }
}