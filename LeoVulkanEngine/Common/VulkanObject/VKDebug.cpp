#include "VKDebug.hpp"

namespace LeoVK::Debug
{
    VkBool32 MessageCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t srcObject,
                             size_t location, int32_t msgCode, const char *pLayerPrefix, const char *pMsg,
                             void *pUserData) {
        return 0;
    }

    void SetupDebugging(VkInstance instance) {

    }

    void FreeDebugCallback(VkInstance instance) {

    }
}


namespace LeoVK::DebugMarker
{

    void Setup(VkDevice device) {

    }

    void SetObjectName(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType,
                                           const char *name) {

    }

    void SetObjectTag(VkDevice device, uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name,
                                     size_t tagSize, const void *tag) {

    }

    void BeginRegion(VkCommandBuffer cmdbuffer, const char *pMarkerName, glm::vec4 color) {

    }

    void Insert(VkCommandBuffer cmdbuffer, std::string markerName, glm::vec4 color) {

    }

    void EndRegion(VkCommandBuffer cmdBuffer) {

    }

    void SetCommandBufferName(VkDevice device, VkCommandBuffer cmdBuffer, const char *name) {

    }

    void SetQueueName(VkDevice device, VkQueue queue, const char *name) {

    }

    void SetImageName(VkDevice device, VkImage image, const char *name) {

    }

    void SetSamplerName(VkDevice device, VkSampler sampler, const char *name) {

    }

    void SetBufferName(VkDevice device, VkBuffer buffer, const char *name) {

    }

    void SetDeviceMemoryName(VkDevice device, VkDeviceMemory memory, const char *name) {

    }

    void SetShaderModuleName(VkDevice device, VkShaderModule shaderModule, const char *name) {

    }

    void SetPipelineName(VkDevice device, VkPipeline pipeline, const char *name) {

    }

    void SetPipelineLayoutName(VkDevice device, VkPipelineLayout pipelineLayout, const char *name) {

    }

    void SetRenderPassName(VkDevice device, VkRenderPass renderPass, const char *name) {

    }

    void SetFramebufferName(VkDevice device, VkFramebuffer framebuffer, const char *name) {

    }

    void SetDescriptorSetLayoutName(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                                        const char *name) {

    }

    void SetDescriptorSetName(VkDevice device, VkDescriptorSet descriptorSet, const char *name) {

    }

    void SetSemaphoreName(VkDevice device, VkSemaphore semaphore, const char *name) {

    }

    void SetFenceName(VkDevice device, VkFence fence, const char *name) {

    }

    void SetEventName(VkDevice device, VkEvent _event, const char *name) {

    }
}