#include "Pipeline.h"
#include "../core/VulkanContext.h"
#include "../core/SwapChain.h"
#include <iostream>
#include <fstream>

namespace voxceleron {

Pipeline::Pipeline(VulkanContext* context, SwapChain* swapChain)
    : context(context)
    , swapChain(swapChain)
    , pipelineLayout(VK_NULL_HANDLE)
    , graphicsPipeline(VK_NULL_HANDLE)
    , renderPass(VK_NULL_HANDLE)
    , commandPool(VK_NULL_HANDLE)
    , currentFrame(0) {
    std::cout << "Pipeline: Creating pipeline instance" << std::endl;
}

Pipeline::~Pipeline() {
    std::cout << "Pipeline: Destroying pipeline instance" << std::endl;
}

bool Pipeline::initialize() {
    std::cout << "Pipeline: Starting initialization..." << std::endl;

    if (!createRenderPass()) {
        std::cerr << "Pipeline: Failed to create render pass!" << std::endl;
        return false;
    }
    std::cout << "Pipeline: Render pass created successfully" << std::endl;

    if (!createGraphicsPipeline()) {
        std::cerr << "Pipeline: Failed to create graphics pipeline!" << std::endl;
        return false;
    }
    std::cout << "Pipeline: Graphics pipeline created successfully" << std::endl;

    if (!createFramebuffers()) {
        std::cerr << "Pipeline: Failed to create framebuffers!" << std::endl;
        return false;
    }
    std::cout << "Pipeline: Framebuffers created successfully" << std::endl;

    if (!createCommandPool()) {
        std::cerr << "Pipeline: Failed to create command pool!" << std::endl;
        return false;
    }
    std::cout << "Pipeline: Command pool created successfully" << std::endl;

    if (!createCommandBuffers()) {
        std::cerr << "Pipeline: Failed to create command buffers!" << std::endl;
        return false;
    }
    std::cout << "Pipeline: Command buffers created successfully" << std::endl;

    if (!createSyncObjects()) {
        std::cerr << "Pipeline: Failed to create synchronization objects!" << std::endl;
        return false;
    }
    std::cout << "Pipeline: Synchronization objects created successfully" << std::endl;

    std::cout << "Pipeline: Initialization complete" << std::endl;
    return true;
}

void Pipeline::cleanup() {
    std::cout << "Pipeline: Starting cleanup..." << std::endl;

    vkDeviceWaitIdle(context->getDevice());

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (renderFinishedSemaphores[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(context->getDevice(), renderFinishedSemaphores[i], nullptr);
        if (imageAvailableSemaphores[i] != VK_NULL_HANDLE)
            vkDestroySemaphore(context->getDevice(), imageAvailableSemaphores[i], nullptr);
        if (inFlightFences[i] != VK_NULL_HANDLE)
            vkDestroyFence(context->getDevice(), inFlightFences[i], nullptr);
    }
    std::cout << "Pipeline: Destroyed synchronization objects" << std::endl;

    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context->getDevice(), commandPool, nullptr);
        std::cout << "Pipeline: Destroyed command pool" << std::endl;
    }

    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(context->getDevice(), framebuffer, nullptr);
    }
    std::cout << "Pipeline: Destroyed " << framebuffers.size() << " framebuffers" << std::endl;
    framebuffers.clear();

    if (graphicsPipeline != VK_NULL_HANDLE) {
        std::cout << "Pipeline: Destroying graphics pipeline" << std::endl;
        vkDestroyPipeline(context->getDevice(), graphicsPipeline, nullptr);
        graphicsPipeline = VK_NULL_HANDLE;
    }

    if (pipelineLayout != VK_NULL_HANDLE) {
        std::cout << "Pipeline: Destroying pipeline layout" << std::endl;
        vkDestroyPipelineLayout(context->getDevice(), pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    if (renderPass != VK_NULL_HANDLE) {
        std::cout << "Pipeline: Destroying render pass" << std::endl;
        vkDestroyRenderPass(context->getDevice(), renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }

    std::cout << "Pipeline: Cleanup complete" << std::endl;
}

bool Pipeline::beginFrame() {
    std::cout << "Pipeline: Beginning frame " << currentFrame << std::endl;
    
    std::cout << "Pipeline: Waiting for fence..." << std::endl;
    vkWaitForFences(context->getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    std::cout << "Pipeline: Acquiring next image..." << std::endl;
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context->getDevice(), swapChain->getHandle(), UINT64_MAX,
        imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        std::cout << "Pipeline: Swap chain out of date, recreating..." << std::endl;
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "Pipeline: Failed to acquire swap chain image! Error: " << result << std::endl;
        return false;
    }
    std::cout << "Pipeline: Acquired image index: " << imageIndex << std::endl;

    std::cout << "Pipeline: Resetting fence..." << std::endl;
    vkResetFences(context->getDevice(), 1, &inFlightFences[currentFrame]);

    std::cout << "Pipeline: Resetting command buffer..." << std::endl;
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    std::cout << "Pipeline: Recording command buffer..." << std::endl;
    if (!recordCommandBuffer(commandBuffers[currentFrame], imageIndex)) {
        std::cerr << "Pipeline: Failed to record command buffer!" << std::endl;
        return false;
    }

    std::cout << "Pipeline: Preparing submit info..." << std::endl;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    std::cout << "Pipeline: Submitting command buffer..." << std::endl;
    if (vkQueueSubmit(context->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        std::cerr << "Pipeline: Failed to submit draw command buffer!" << std::endl;
        return false;
    }

    std::cout << "Pipeline: Preparing present info..." << std::endl;
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain->getHandle()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    std::cout << "Pipeline: Presenting frame..." << std::endl;
    result = vkQueuePresentKHR(context->getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        std::cout << "Pipeline: Swap chain out of date or suboptimal, recreating..." << std::endl;
        return false;
    } else if (result != VK_SUCCESS) {
        std::cerr << "Pipeline: Failed to present swap chain image! Error: " << result << std::endl;
        return false;
    }

    std::cout << "Pipeline: Frame " << currentFrame << " completed successfully" << std::endl;
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
}

void Pipeline::endFrame() {
    std::cout << "Pipeline: Ending frame " << currentFrame << std::endl;
}

bool Pipeline::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    std::cout << "Pipeline: Recording command buffer for image " << imageIndex << std::endl;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    std::cout << "Pipeline: Beginning command buffer..." << std::endl;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "Pipeline: Failed to begin recording command buffer!" << std::endl;
        return false;
    }

    std::cout << "Pipeline: Setting up render pass..." << std::endl;
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChain->getExtent();

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.2f, 1.0f}}}; // Changed to dark blue for visibility
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    std::cout << "Pipeline: Beginning render pass..." << std::endl;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    std::cout << "Pipeline: Binding pipeline..." << std::endl;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    std::cout << "Pipeline: Recording draw command..." << std::endl;
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    std::cout << "Pipeline: Ending render pass..." << std::endl;
    vkCmdEndRenderPass(commandBuffer);

    std::cout << "Pipeline: Ending command buffer..." << std::endl;
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cerr << "Pipeline: Failed to record command buffer!" << std::endl;
        return false;
    }

    std::cout << "Pipeline: Command buffer recorded successfully" << std::endl;
    return true;
}

bool Pipeline::createCommandPool() {
    std::cout << "Pipeline: Creating command pool..." << std::endl;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context->getGraphicsQueueFamily();

    if (vkCreateCommandPool(context->getDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::cerr << "Pipeline: Failed to create command pool!" << std::endl;
        return false;
    }

    return true;
}

bool Pipeline::createCommandBuffers() {
    std::cout << "Pipeline: Creating command buffers..." << std::endl;

    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(context->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "Pipeline: Failed to allocate command buffers!" << std::endl;
        return false;
    }

    std::cout << "Pipeline: Created " << commandBuffers.size() << " command buffers" << std::endl;
    return true;
}

bool Pipeline::createSyncObjects() {
    std::cout << "Pipeline: Creating synchronization objects..." << std::endl;

    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(context->getDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(context->getDevice(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(context->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            std::cerr << "Pipeline: Failed to create synchronization objects for frame " << i << std::endl;
            return false;
        }
    }

    std::cout << "Pipeline: Created synchronization objects for " << MAX_FRAMES_IN_FLIGHT << " frames" << std::endl;
    return true;
}

bool Pipeline::createRenderPass() {
    std::cout << "Pipeline: Creating render pass..." << std::endl;

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChain->getImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(context->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        std::cerr << "Pipeline: Failed to create render pass!" << std::endl;
        return false;
    }

    return true;
}

bool Pipeline::createGraphicsPipeline() {
    std::cout << "Pipeline: Creating graphics pipeline..." << std::endl;

    // Read shader files
    std::cout << "Pipeline: Reading shader files..." << std::endl;
    auto vertShaderCode = readFile("shaders/basic.vert.spv");
    auto fragShaderCode = readFile("shaders/basic.frag.spv");

    if (vertShaderCode.empty() || fragShaderCode.empty()) {
        std::cerr << "Pipeline: Failed to read shader files!" << std::endl;
        return false;
    }
    std::cout << "Pipeline: Shader files read successfully" << std::endl;

    // Create shader modules
    std::cout << "Pipeline: Creating shader modules..." << std::endl;
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
        std::cerr << "Pipeline: Failed to create shader modules!" << std::endl;
        return false;
    }
    std::cout << "Pipeline: Shader modules created successfully" << std::endl;

    // Shader stage creation
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChain->getExtent().width);
    viewport.height = static_cast<float>(swapChain->getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChain->getExtent();

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (vkCreatePipelineLayout(context->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        std::cerr << "Pipeline: Failed to create pipeline layout!" << std::endl;
        return false;
    }
    std::cout << "Pipeline: Pipeline layout created successfully" << std::endl;

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(context->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        std::cerr << "Pipeline: Failed to create graphics pipeline!" << std::endl;
        return false;
    }
    std::cout << "Pipeline: Graphics pipeline created successfully" << std::endl;

    // Cleanup shader modules
    vkDestroyShaderModule(context->getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(context->getDevice(), vertShaderModule, nullptr);
    std::cout << "Pipeline: Shader modules cleaned up" << std::endl;

    return true;
}

bool Pipeline::createFramebuffers() {
    std::cout << "Pipeline: Creating framebuffers..." << std::endl;

    const auto& swapChainImageViews = swapChain->getImageViews();
    framebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChain->getExtent().width;
        framebufferInfo.height = swapChain->getExtent().height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(context->getDevice(), &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            std::cerr << "Pipeline: Failed to create framebuffer " << i << std::endl;
            return false;
        }
    }

    std::cout << "Pipeline: Created " << framebuffers.size() << " framebuffers" << std::endl;
    return true;
}

std::vector<char> Pipeline::readFile(const std::string& filename) {
    std::cout << "Pipeline: Reading file: " << filename << std::endl;

    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Pipeline: Failed to open file: " << filename << std::endl;
        return {};
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    std::cout << "Pipeline: Read " << fileSize << " bytes from " << filename << std::endl;
    return buffer;
}

VkShaderModule Pipeline::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(context->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::cerr << "Pipeline: Failed to create shader module!" << std::endl;
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

} // namespace voxceleron 