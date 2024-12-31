#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include "engine/Engine.h"
#include "engine/Renderer.h"
#include "engine/World.h"
#include <vector>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <sstream>
#include <iomanip>

void renderUI(const Renderer& renderer) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Create a window in the top-right corner
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 250, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(240, 100), ImGuiCond_Always);
    ImGui::Begin("Statistics", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | 
                                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    // Display FPS with 1 decimal place
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << renderer.getFPS();
    ImGui::Text("FPS: %s", ss.str().c_str());

    // Display face statistics
    uint32_t renderedFaces = renderer.getTotalFaces() - renderer.getCulledFaces();
    ImGui::Text("Faces: %u / %u (Culled: %u)", 
        renderedFaces, renderer.getTotalFaces(), renderer.getCulledFaces());

    // Display total voxels
    ImGui::Text("Total Voxels: %u", renderer.getTotalVoxels());

    ImGui::End();
    ImGui::Render();
}

int main() {
    try {
        // Initialize GLFW
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        // Create window
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        GLFWwindow* window = glfwCreateWindow(800, 600, "Voxceron Engine", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create window");
        }

        // Initialize ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForVulkan(window, true);

        // Setup Vulkan instance
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Voxceron";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Voxceron Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // Get required extensions for GLFW and ImGui
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        
        // Enable validation layers in debug builds
        #ifdef NDEBUG
            const bool enableValidationLayers = false;
        #else
            const bool enableValidationLayers = true;
        #endif

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = 1;
            const char* validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
            createInfo.ppEnabledLayerNames = validationLayers;
        } else {
            createInfo.enabledLayerCount = 0;
        }

        VkInstance instance;
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance");
        }

        // Create surface
        VkSurfaceKHR surface;
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface");
        }

        // Initialize renderer
        Renderer renderer;
        renderer.init(instance, surface);

        // Initialize ImGui Vulkan implementation
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = instance;
        init_info.PhysicalDevice = renderer.getPhysicalDevice();
        init_info.Device = renderer.getDevice();
        init_info.Queue = renderer.getGraphicsQueue();
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = renderer.getDescriptorPool();
        init_info.MinImageCount = 2;
        init_info.ImageCount = 2;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = nullptr;
        init_info.RenderPass = renderer.getRenderPass();
        ImGui_ImplVulkan_Init(&init_info);

        // Create and initialize world
        World world;
        world.generateTestWorld();
        renderer.setWorld(&world);
        renderer.updateWorldMesh();

        // Main loop
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            
            // Render world
            renderer.draw();
            
            // Render UI
            renderUI(renderer);
        }

        // Cleanup
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        renderer.cleanup();
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}