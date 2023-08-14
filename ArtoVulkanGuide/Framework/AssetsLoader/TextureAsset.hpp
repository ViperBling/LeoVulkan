#pragma once

#include "AssetsLoader.hpp"

namespace Assets
{
    enum class TextureFormat : uint32_t
    {
        Unknown = 0,
        RGBA8
    };

    struct PageInfo
    {
        uint32_t mWidth;
        uint32_t mHeight;
        uint32_t mCompressedSize;
        uint32_t mOriginalSize;
    };

    struct TextureInfo
    {
        uint64_t mTexSize;
        TextureFormat mTexFormat;
        CompressionMode mCompressionMode;

        std::string mOriginalFile;
        std::vector<PageInfo> mPages;
    };

    TextureInfo ReadTextureInfo(AssetFile* file);
    void UnpackTexture(TextureInfo* info, const char* srcBuffer, size_t srcSize, char* dst);
    void UnpackTexturePage(TextureInfo* info, int pageIndex ,char* srcBuffer, char* dst);
    AssetFile PackTexture(TextureInfo* info, void* pixelData);
}