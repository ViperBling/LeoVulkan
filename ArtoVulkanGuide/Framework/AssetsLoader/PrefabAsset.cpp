#include "json.hpp"
#include "lz4.h"

#include "PrefabAsset.hpp"

namespace Assets
{
    PrefabInfo ReadPrefabInfo(AssetFile *file)
    {
        PrefabInfo info;
        nlohmann::json metaData = nlohmann::json::parse(file->mJs);

        for (auto& pair : metaData["NodeMatrices"].items())
        {
            auto value = pair.value();
            info.mNodeMatrices[value[0]] = value[1];
        }

        for (auto& [key, value]  : metaData["NodeNames"].items())
        {
            info.mNodeNames[value[0]] = value[1];
        }

        for (auto& [key, value] : metaData["NodeParents"].items())
        {
            info.mNodeParents[value[0]] = value[1];
        }

        std::unordered_map<uint64_t, nlohmann::json> meshNodes = metaData["NodeMeshes"];

        for (auto pair : meshNodes)
        {
            Assets::PrefabInfo::NodeMesh node;

            node.mMeshPath = pair.second["MeshPath"];
            node.mMaterialPath = pair.second["MaterialPath"];

            info.mNodeMeshes[pair.first] = node;
        }

        size_t numMatrices = file->mBinaryBlob.size() / (sizeof(float) * 16);
        info.mMatrices.resize(numMatrices);

        memcpy(info.mMatrices.data(),file->mBinaryBlob.data(), file->mBinaryBlob.size());

        return info;
    }

    AssetFile PackPrefab(const PrefabInfo &info)
    {
        nlohmann::json metaData;
        metaData["NodeMatrices"] = info.mNodeMatrices;
        metaData["NodeNames"]    = info.mNodeNames;
        metaData["NodeParents"]  = info.mNodeParents;

        std::unordered_map<uint64_t, nlohmann::json> meshIndex;
        for (auto& pair : info.mNodeMeshes)
        {
            nlohmann::json meshNode;
            meshNode["MeshPath"] = pair.second.mMeshPath;
            meshNode["MaterialPath"] = pair.second.mMaterialPath;
            meshIndex[pair.first] = meshNode;
        }

        metaData["NodeMeshes"] = meshIndex;

        //core file header
        AssetFile file;
        file.mType[0] = 'P';
        file.mType[1] = 'R';
        file.mType[2] = 'F';
        file.mType[3] = 'B';
        file.mVersion = 1;

        file.mBinaryBlob.resize(info.mMatrices.size() * sizeof(float) * 16);
        memcpy(file.mBinaryBlob.data(), info.mMatrices.data(), info.mMatrices.size() * sizeof(float) * 16);

        std::string stringField = metaData.dump();
        file.mJs = stringField;

        return file;
    }
}