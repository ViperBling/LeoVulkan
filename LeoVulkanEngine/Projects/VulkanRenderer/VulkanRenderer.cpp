#include "VulkanRenderer.h"


VulkanRenderer::VulkanRenderer(bool msaa, bool enableValidation)
{

}

VulkanRenderer::~VulkanRenderer()
{

}

void VulkanRenderer::GetEnabledFeatures()
{
    VKRendererBase::GetEnabledFeatures();
}

void VulkanRenderer::BuildCommandBuffers()
{
    VKRendererBase::BuildCommandBuffers();
}

void VulkanRenderer::Render()
{

}

void VulkanRenderer::ViewChanged()
{
    VKRendererBase::ViewChanged();
}

void VulkanRenderer::OnUpdateUIOverlay(LeoVK::UIOverlay *overlay)
{
    VKRendererBase::OnUpdateUIOverlay(overlay);
}

void VulkanRenderer::LoadScene(std::string filename)
{
    std::cout << "Loading scene from: " << filename << std::endl;
    mRenderScene.mScene.Destroy(mDevice);
    auto tStart = std::chrono::high_resolution_clock::now();
    mRenderScene.mScene.LoadFromFile(filename, mpVulkanDevice, mQueue);
    auto tFileLoad = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - tStart).count();
    std::cout << "Loading took " << tFileLoad << " ms" << std::endl;
}

void VulkanRenderer::LoadAssets()
{

}

void VulkanRenderer::SetupDescriptors()
{

}

void VulkanRenderer::PreparePipelines()
{

}

void VulkanRenderer::PrepareUniformBuffers()
{

}

void VulkanRenderer::UpdateUniformBuffers()
{

}

void VulkanRenderer::PreRender()
{

}


