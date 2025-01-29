#include "Pipeline.h"
#include "../core/VulkanContext.h"
#include "../core/SwapChain.h"
#include "../../core/Window.h"
#include <iostream>
#include <array>

namespace voxceleron {

Pipeline::Pipeline(VulkanContext* context, SwapChain* swapChain)
    : context(context)
    , swapChain(swapChain)
    , window(nullptr)
    , pipelineLayout(VK_NULL_HANDLE)
    , graphicsPipeline(VK_NULL_HANDLE)
    , renderPass(VK_NULL_HANDLE)
    , currentFrame(0)
    , currentImageIndex(0)
    , state(State::UNINITIALIZED)
    , waitStageFlags(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) {
    std::cout << "Pipeline: Creating pipeline instance" << std::endl;
}

Pipeline::~Pipeline() {
    std::cout << "Pipeline: Destroying pipeline instance" << std::endl;
    cleanup();
}

bool Pipeline::initialize() {
    if (state != State::UNINITIALIZED) {
        setError("Pipeline is already initialized");
        return false;
    }

    if (!createRenderPass() ||
        !createGraphicsPipeline() ||
        !createFramebuffers() ||
        !createCommandPools() ||
        !createCommandBuffers() ||
        !createSyncObjects() ||
        !createVertexBuffer()) {
        return false;
    }

    state = State::READY;
    return true;
}

void Pipeline::cleanup() {
    std::cout << "Pipeline: Starting cleanup..." << std::endl;
    waitIdle();

    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context->getDevice(), vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }

    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context->getDevice(), vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
    }

    // Clean up synchronization objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (imageAvailableSemaphores.size() > i) {
            vkDestroySemaphore(context->getDevice(), imageAvailableSemaphores[i], nullptr);
        }
        if (renderFinishedSemaphores.size() > i) {
            vkDestroySemaphore(context->getDevice(), renderFinishedSemaphores[i], nullptr);
        }
        if (inFlightFences.size() > i) {
            vkDestroyFence(context->getDevice(), inFlightFences[i], nullptr);
        }
    }

    // Clean up command buffers and pools
    for (size_t i = 0; i < commandPools.size(); i++) {
        if (commandPools[i] != VK_NULL_HANDLE) {
            vkDestroyCommandPool(context->getDevice(), commandPools[i], nullptr);
        }
    }

    // Clean up framebuffers first
    std::cout << "Pipeline: Cleaning up " << framebuffers.size() << " framebuffers" << std::endl;
    for (auto& framebuffer : framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            std::cout << "Pipeline: Destroying framebuffer: " << framebuffer << std::endl;
            vkDestroyFramebuffer(context->getDevice(), framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }
    framebuffers.clear();

    if (graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(context->getDevice(), graphicsPipeline, nullptr);
        graphicsPipeline = VK_NULL_HANDLE;
    }

    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context->getDevice(), pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }

    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context->getDevice(), renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }

    imageAvailableSemaphores.clear();
    renderFinishedSemaphores.clear();
    inFlightFences.clear();
    commandBuffers.clear();
    commandPools.clear();

    state = State::UNINITIALIZED;
    std::cout << "Pipeline: Cleanup complete" << std::endl;
}

bool Pipeline::beginFrame() {
    if (state != State::READY) {
        setError("Pipeline is not in ready state");
        return false;
    }
    
    // Wait for previous frame
    vkWaitForFences(context->getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // Acquire next image
    VkResult result = vkAcquireNextImageKHR(
        context->getDevice(),
        swapChain->getHandle(),
        UINT64_MAX,
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE,
        &currentImageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        state = State::RECREATING;
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        setError("Failed to acquire swap chain image");
        return false;
    }

    // Reset fence only if we are submitting work
    vkResetFences(context->getDevice(), 1, &inFlightFences[currentFrame]);

    // Reset command buffer
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffers[currentFrame], &beginInfo) != VK_SUCCESS) {
        setError("Failed to begin recording command buffer");
        return false;
    }

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffers[currentImageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChain->getExtent();

    // Clear color (black)
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the graphics pipeline
    vkCmdBindPipeline(commandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    // Bind vertex buffer and draw
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers[currentFrame], 0, 1, vertexBuffers, offsets);
    vkCmdDraw(commandBuffers[currentFrame], 3, 1, 0, 0);

    return true;
}

bool Pipeline::endFrame() {
    if (state != State::READY) {
        setError("Pipeline is not in ready state");
        return false;
    }

    vkCmdEndRenderPass(commandBuffers[currentFrame]);

    if (vkEndCommandBuffer(commandBuffers[currentFrame]) != VK_SUCCESS) {
        setError("Failed to record command buffer");
        return false;
    }

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
    submitInfo.pWaitDstStageMask = &waitStageFlags;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];

    if (vkQueueSubmit(context->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        setError("Failed to submit draw command buffer");
        return false;
    }

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
    presentInfo.swapchainCount = 1;
    VkSwapchainKHR swapChainHandle = swapChain->getHandle();
    presentInfo.pSwapchains = &swapChainHandle;
    presentInfo.pImageIndices = &currentImageIndex;

    VkResult result = vkQueuePresentKHR(context->getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        state = State::RECREATING;
    } else if (result != VK_SUCCESS) {
        setError("Failed to present swap chain image");
        return false;
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    return true;
}

bool Pipeline::recreateIfNeeded() {
    if (state != State::RECREATING) {
        return true;
    }

    std::cout << "Pipeline: Starting recreation..." << std::endl;
    
    // Wait for device to be idle before cleanup
    waitIdle();
    
    // Store old framebuffers for cleanup
    std::vector<VkFramebuffer> oldFramebuffers = framebuffers;
    framebuffers.clear();

    // Clean up old framebuffers explicitly
    for (auto& fb : oldFramebuffers) {
        if (fb != VK_NULL_HANDLE) {
            std::cout << "Pipeline: Cleaning up old framebuffer during recreation: " << fb << std::endl;
            vkDestroyFramebuffer(context->getDevice(), fb, nullptr);
        }
    }

    // Perform full cleanup and reinitialize
    cleanup();
    bool result = initialize();
    
    if (result) {
        std::cout << "Pipeline: Recreation successful" << std::endl;
    } else {
        std::cerr << "Pipeline: Recreation failed" << std::endl;
    }
    
    return result;
}

void Pipeline::waitIdle() {
    if (context && context->getDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context->getDevice());
    }
}

VkCommandBuffer Pipeline::getCurrentCommandBuffer() const {
    return commandBuffers[currentFrame];
}

bool Pipeline::createRenderPass() {
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

    VkSubpassDependency dependencies[2];
    
    // Transition from undefined to color attachment optimal
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = 0;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = 0;

    // Transition from color attachment optimal to present
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = 0;
    dependencies[1].dependencyFlags = 0;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = dependencies;

    if (vkCreateRenderPass(context->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        setError("Failed to create render pass");
        return false;
    }

    return true;
}

bool Pipeline::createGraphicsPipeline() {
    // Load shader code
    std::vector<char> vertShaderCode = readFile("shaders/basic.vert.spv");
    std::vector<char> fragShaderCode = readFile("shaders/basic.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
        setError("Failed to create shader modules");
        return false;
    }

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    if (vkCreatePipelineLayout(context->getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        setError("Failed to create pipeline layout");
        return false;
    }

    // Shader stages
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
    
    // Vertex input state
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, pos);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32_UINT;
    attributeDescriptions[1].offset = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    
    // Input assembly state
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;
    
    // Viewport state
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
    
    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    
    // Multisampling state
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    
    // Color blend state
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    
    // Create the graphics pipeline
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
    
    if (vkCreateGraphicsPipelines(context->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        setError("Failed to create graphics pipeline");
        vkDestroyShaderModule(context->getDevice(), fragShaderModule, nullptr);
        vkDestroyShaderModule(context->getDevice(), vertShaderModule, nullptr);
        return false;
    }
    
    // Clean up shader modules
    vkDestroyShaderModule(context->getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(context->getDevice(), vertShaderModule, nullptr);
    
    return true;
}

bool Pipeline::createFramebuffers() {
    // Get swap chain images
    std::vector<VkImage> swapChainImages = swapChain->getImages();
    std::vector<VkImageView> swapChainImageViews = swapChain->getImageViews();
    VkExtent2D swapChainExtent = swapChain->getExtent();

    // Create a framebuffer for each swap chain image view
    framebuffers.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageView attachments[] = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(context->getDevice(), &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS) {
            setError("Failed to create framebuffer");
            return false;
        }
    }

    return true;
}

bool Pipeline::createCommandPools() {
    // Create a command pool for each frame in flight
    commandPools.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context->getGraphicsQueueFamily();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateCommandPool(context->getDevice(), &poolInfo, nullptr, &commandPools[i]) != VK_SUCCESS) {
            setError("Failed to create command pool");
            return false;
        }
    }

    return true;
}

bool Pipeline::createCommandBuffers() {
    // Create a command buffer for each frame in flight
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        allocInfo.commandPool = commandPools[i];
        if (vkAllocateCommandBuffers(context->getDevice(), &allocInfo, &commandBuffers[i]) != VK_SUCCESS) {
            setError("Failed to allocate command buffers");
            return false;
        }
    }

    return true;
}

bool Pipeline::createSyncObjects() {
    // Resize vectors to hold sync objects for each frame in flight
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
            setError("Failed to create synchronization objects");
            return false;
        }
    }

    return true;
}

bool Pipeline::createVertexBuffer() {
    std::array<Vertex, 3> vertices = {{
        {{0.0f, -0.5f, 0.0f}, 0},  // Bottom
        {{0.5f, 0.5f, 0.0f}, 1},   // Top right
        {{-0.5f, 0.5f, 0.0f}, 2}   // Top left
    }};

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(vertices[0]) * vertices.size();
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(context->getDevice(), &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
        setError("Failed to create vertex buffer");
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context->getDevice(), vertexBuffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = context->findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (vkAllocateMemory(context->getDevice(), &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
        setError("Failed to allocate vertex buffer memory");
        return false;
    }

    vkBindBufferMemory(context->getDevice(), vertexBuffer, vertexBufferMemory, 0);

    void* data;
    vkMapMemory(context->getDevice(), vertexBufferMemory, 0, bufferInfo.size, 0, &data);
    memcpy(data, vertices.data(), bufferInfo.size);
    vkUnmapMemory(context->getDevice(), vertexBufferMemory);

    return true;
}

VkShaderModule Pipeline::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(context->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    return shaderModule;
}

std::vector<char> Pipeline::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        setError("Failed to open file: " + filename);
        return std::vector<char>();
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

} // namespace voxceleron 