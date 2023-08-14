#pragma once

#include <unordered_map>

#include "AssetsLoader.hpp"

namespace Assets
{
    enum class TransparencyMode : uint8_t
    {
        Opaque,
        Transparent,
        Masked
    };

    struct MaterialInfo
    {
        std::string mBaseEffect;
        std::unordered_map<std::string, std::string> mTextures; //name -> path
        std::unordered_map<std::string, std::string> mCustomProps;
        TransparencyMode mTransparency;
    };

    MaterialInfo ReadMaterialInfo(AssetFile* file);
    AssetFile PackMaterial(MaterialInfo* info);
}