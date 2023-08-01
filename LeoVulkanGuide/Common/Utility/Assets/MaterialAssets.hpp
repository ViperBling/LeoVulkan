#pragma once

#include "AssetsLoader.hpp"
#include <unordered_map>

namespace Assets
{
    enum class TransparentMode:uint8_t
    {
        Opaque,
        Transparent,
        Masked
    };

    struct MaterialInfo
    {
        std::string mBaseEffect;
        std::unordered_map<std::string, std::string> mTextures;
        std::unordered_map<std::string, std::string> mCustomProps;
        TransparentMode mTransparency;
    };

    MaterialInfo ReadMaterialInfo(AssetFile* file);
    AssetFile PackMaterial(MaterialInfo* info);
}