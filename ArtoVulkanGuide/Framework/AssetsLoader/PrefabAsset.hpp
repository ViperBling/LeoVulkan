#pragma once

#include <unordered_map>

#include "AssetsLoader.hpp"

namespace Assets
{
    struct PrefabInfo
    {
        //points to matrix array in the blob
        std::unordered_map<uint64_t, int> mNodeMatrices;
        std::unordered_map<uint64_t, std::string> mNodeNames;
        std::unordered_map<uint64_t, uint64_t> mNodeParents;

        struct NodeMesh
        {
            std::string mMaterialPath;
            std::string mMeshPath;
        };

        std::unordered_map<uint64_t, NodeMesh> mNodeMeshes;
        std::vector<std::array<float, 16>> mMatrices;
    };

    PrefabInfo ReadPrefabInfo(AssetFile* file);
    AssetFile PackPrefab(const PrefabInfo& info);
}