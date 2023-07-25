#pragma once

#include "ProjectPCH.hpp"

#include "VKRendererBase.hpp"

#define ENABLE_VALIDATION true
#define ENABLE_MSAA true

struct RenderScene
{
    LeoVK::GLTFScene mScene;
    LeoVK::GLTFScene mSkybox;
};

class VulkanRenderer : public VKRendererBase
{
public:
    VulkanRenderer(bool msaa, bool enableValidation);
    virtual ~VulkanRenderer();
    void GetEnabledFeatures() override;
    void BuildCommandBuffers() override;
    void Render() override;
    void ViewChanged() override;
    void OnUpdateUIOverlay(LeoVK::UIOverlay* overlay) override;

    void LoadScene(std::string filename);
    void LoadAssets();
    void SetupDescriptors();
    void PreparePipelines();
    void PrepareUniformBuffers();
    void UpdateUniformBuffers();
    void PreRender();

public:
    RenderScene mRenderScene;

};
