#include "json.hpp"
#include "lz4.h"

#include "MaterialAsset.hpp"

namespace Assets
{
    MaterialInfo ReadMaterialInfo(AssetFile *file)
    {
        Assets::MaterialInfo info;

        nlohmann::json matMetaData = nlohmann::json::parse(file->mJs);
        info.mBaseEffect = matMetaData["BaseEffect"];

        for (auto& [key, value] : matMetaData["Textures"].items())
        {
            info.mTextures[key] = value;
        }
        for (auto& [key, value] : matMetaData["CustomProperties"].items())
        {
            info.mCustomProps[key] = value;
        }

        info.mTransparency = TransparencyMode::Opaque;

        auto it = matMetaData.find("Transparency");
        if (it != matMetaData.end())
        {
            std::string val = (*it);
            if (val == "Transparent")
            {
                info.mTransparency = TransparencyMode::Transparent;
            }
            if (val == "Masked")
            {
                info.mTransparency = TransparencyMode::Masked;
            }
        }

        return info;
    }

    AssetFile PackMaterial(MaterialInfo *info)
    {
        nlohmann::json matMetaData;
        matMetaData["BaseEffect"] = info->mBaseEffect;
        matMetaData["Textures"] = info->mTextures;
        matMetaData["CustomProperties"] = info->mCustomProps;

        switch (info->mTransparency)
        {
            case TransparencyMode::Transparent:
                matMetaData["Transparency"] = "Transparent";
                break;
            case TransparencyMode::Masked:
                matMetaData["Transparency"] = "Masked";
                break;
        }

        //core file header
        AssetFile file;
        file.mType[0] = 'M';
        file.mType[1] = 'A';
        file.mType[2] = 'T';
        file.mType[3] = 'X';
        file.mVersion = 1;

        std::string stringFiled = matMetaData.dump();
        file.mJs = stringFiled;

        return file;
    }
}