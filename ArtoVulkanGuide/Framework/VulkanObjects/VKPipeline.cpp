#include <iostream>

#include "VKPipeline.hpp"

VkPipeline PipelineBuilder::BuildPipeline(VkDevice device, VkRenderPass renderPass)
{
    VkPipelineViewportStateCreateInfo vpStateCI {};
    vpStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpStateCI.pNext = nullptr;
    vpStateCI.viewportCount = 1;
    vpStateCI.pViewports = &mViewport;
    vpStateCI.scissorCount = 1;
    vpStateCI.pScissors = &mScissor;

    // 暂时不用Color Blending，设一个占位
    VkPipelineColorBlendStateCreateInfo cbStateCI {};
    cbStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbStateCI.pNext = nullptr;
    cbStateCI.logicOpEnable = VK_FALSE;
    cbStateCI.logicOp = VK_LOGIC_OP_COPY;
    cbStateCI.attachmentCount = 1;
    cbStateCI.pAttachments = &mCBAttach;

    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.pNext = nullptr;
    pipelineCI.stageCount = mShaderStageCIs.size();
    pipelineCI.pStages = mShaderStageCIs.data();
    pipelineCI.pVertexInputState = &mVIState;
    pipelineCI.pInputAssemblyState = &mIAState;
    pipelineCI.pViewportState = &vpStateCI;
    pipelineCI.pRasterizationState = &mRSState;
    pipelineCI.pMultisampleState = &mMSState;
    pipelineCI.pColorBlendState = &cbStateCI;
    pipelineCI.layout = mPipelineLayout;
    pipelineCI.renderPass = renderPass;
    pipelineCI.subpass = 0;
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline  newPipeline;
    if (vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &newPipeline) != VK_SUCCESS)
    {
        std::cout << "Fail to create pipeline\n";
        return VK_NULL_HANDLE;
    }
    else
    {
        return newPipeline;
    }
}
