#pragma once

#include "VKTypes.hpp"
#include "VKEngine.hpp"

namespace VKUtil
{
    bool LoadImageFromFile(VulkanEngine& engine, const std::string& filename, AllocatedImage& outImage);
}