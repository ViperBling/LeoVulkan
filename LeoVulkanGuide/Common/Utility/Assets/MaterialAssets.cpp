#include "MaterialAssets.hpp"
#include "json.hpp"
#include "lz4.h"

namespace Assets
{
    MaterialInfo ReadMaterialInfo(AssetFile* file)
    {
        Assets::MaterialInfo info;

        nlohmann::json matJs = nlohmann::json::parse(file->mJs);
        info.mBaseEffect = matJs["baseEffect"];

        for (auto& [key, value] : matJs["textures"].items())
        {
            info.mTextures[key] = value;
        }

        for (auto& [key, value] : matJs["customProps"].items())
        {
            info.mCustomProps[key] = value;
        }

        info.mTransparency = TransparentMode::Opaque;

        auto it = matJs.find("transparency");
        if (it != matJs.end())
        {
            std::string val = (*it);
            if (val == "transparent") info.mTransparency = TransparentMode::Transparent;
            if (val == "masked") info.mTransparency = TransparentMode::Masked;
        }
        return info;
    }

    AssetFile PackMaterial(MaterialInfo* info)
    {
        nlohmann::json matJs;

        matJs["baseEffect"] = info->mBaseEffect;
        matJs["textures"] = info->mTextures;
        matJs["customProps"] = info->mCustomProps;

        switch (info->mTransparency)
        {
            case TransparentMode::Transparent:
                matJs["transparency"] = "transparent";
                break;
            case TransparentMode::Masked:
                matJs["transparency"] = "masked";
                break;
            default:
                break;
        }

        AssetFile file;
        file.mType[0] = 'M';
        file.mType[1] = 'A';
        file.mType[2] = 'T';
        file.mType[3] = 'X';
        file.mVersion = 1;

        std::string strFiled = matJs.dump();
        file.mJs = strFiled;

        return file;
    }
}