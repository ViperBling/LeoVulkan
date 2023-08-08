#pragma once

#include <vector>

#include "VKTypes.hpp"

class PipelineBuilder
{
public:
    VkPipeline BuildPipeline(VkDevice device, VkRenderPass renderPass);

public:
    std::vector<VkPipelineShaderStageCreateInfo> mShaderStageCIs;
    VkPipelineVertexInputStateCreateInfo         mVIState;
    VkPipelineInputAssemblyStateCreateInfo       mIAState;
    VkViewport                                   mViewport;
    VkRect2D                                     mScissor;
    VkPipelineRasterizationStateCreateInfo       mRSState;
    VkPipelineColorBlendAttachmentState          mCBAttach;
    VkPipelineMultisampleStateCreateInfo         mMSState;
    VkPipelineLayout                             mPipelineLayout;
};