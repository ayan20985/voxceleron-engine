cmake_minimum_required(VERSION 3.20)
project(VoxceleronEngine VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add external dependencies
add_subdirectory(external/glfw)
add_subdirectory(external/glm)

# Set source files
set(SOURCES
    src/main.cpp
    src/engine/core/Engine.cpp
    src/engine/core/Window.cpp
    src/engine/vulkan/core/VulkanContext.cpp
    src/engine/vulkan/core/SwapChain.cpp
    src/engine/vulkan/pipeline/Pipeline.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
    ${Vulkan_INCLUDE_DIRS}
)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm
)

# Shader compilation
add_custom_target(shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders ${CMAKE_BINARY_DIR}/shaders
) 