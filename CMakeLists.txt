cmake_minimum_required(VERSION 3.20)
project(VoxceleronEngine VERSION 0.1.0)

# Enable compile commands for IDE with proper caching
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Enable/Disable output of compile commands during generation." FORCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check for Vulkan SDK
if(NOT DEFINED ENV{VULKAN_SDK})
    message(FATAL_ERROR "VULKAN_SDK environment variable is not set. Please install the Vulkan SDK and set the environment variable.")
endif()

# Set Vulkan SDK paths explicitly
set(VULKAN_SDK $ENV{VULKAN_SDK})
set(VULKAN_INCLUDE_DIR "${VULKAN_SDK}/Include")
set(VULKAN_LIB_DIR "${VULKAN_SDK}/Lib")

# Add Vulkan SDK to system include path
include_directories(SYSTEM ${VULKAN_INCLUDE_DIR})

# Windows-specific paths
if(WIN32)
    set(VULKAN_INCLUDE_DIR "${VULKAN_SDK}/Include")
    set(VULKAN_LIB_DIR "${VULKAN_SDK}/Lib")
    set(VULKAN_BIN_DIR "${VULKAN_SDK}/Bin")
endif()

# Add external dependencies
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/CMakeLists.txt")
    message(FATAL_ERROR "GLFW submodule not found. Please run: git submodule update --init --recursive")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/glm/CMakeLists.txt")
    message(FATAL_ERROR "GLM submodule not found. Please run: git submodule update --init --recursive")
endif()

add_subdirectory(external/glfw)
add_subdirectory(external/glm)

# Set source files
set(SOURCES
    src/main.cpp
    src/engine/core/Engine.cpp
    src/engine/core/Window.cpp
    src/engine/core/Camera.cpp
    src/engine/core/InputSystem.cpp
    src/engine/vulkan/core/VulkanContext.cpp
    src/engine/vulkan/core/SwapChain.cpp
    src/engine/vulkan/core/VulkanBuffer.cpp
    src/engine/vulkan/core/VulkanDevice.cpp
    src/engine/vulkan/pipeline/Pipeline.cpp
    src/engine/vulkan/compute/MeshGenerator.cpp
    src/engine/voxel/World.cpp
    src/engine/voxel/WorldRenderer.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories with better organization
set(ENGINE_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
    ${VULKAN_INCLUDE_DIR}
)

# Include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${ENGINE_INCLUDE_DIRS}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/vulkan
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/voxel
)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Vulkan::Vulkan
        glfw
        glm
)

# Add compile definitions for shader paths
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
        VULKAN_SDK_PATH="${VULKAN_SDK}"
)

# Shader handling
set(SHADER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(SHADER_DEBUG_DIR ${CMAKE_CURRENT_BINARY_DIR}/Debug/shaders)

# Create shader directories
file(MAKE_DIRECTORY ${SHADER_BUILD_DIR})
file(MAKE_DIRECTORY ${SHADER_DEBUG_DIR})

# Find all shader source files
file(GLOB SHADER_SOURCES
    ${SHADER_SOURCE_DIR}/*.vert
    ${SHADER_SOURCE_DIR}/*.frag
    ${SHADER_SOURCE_DIR}/*.comp
)

# Function to compile and copy shaders
function(compile_and_copy_shader SHADER)
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    get_filename_component(SHADER_BASENAME ${SHADER} NAME_WE)
    get_filename_component(SHADER_EXT ${SHADER} EXT)

    # Compile shader to SPV
    add_custom_command(
        OUTPUT
            ${SHADER_BUILD_DIR}/${SHADER_NAME}.spv
            ${SHADER_DEBUG_DIR}/${SHADER_NAME}.spv
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER} -o ${SHADER_BUILD_DIR}/${SHADER_NAME}.spv
        COMMAND ${CMAKE_COMMAND} -E copy ${SHADER_BUILD_DIR}/${SHADER_NAME}.spv ${SHADER_DEBUG_DIR}/${SHADER_NAME}.spv
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}"
    )

    # Copy source shader
    add_custom_command(
        OUTPUT
            ${SHADER_BUILD_DIR}/${SHADER_NAME}
            ${SHADER_DEBUG_DIR}/${SHADER_NAME}
        COMMAND ${CMAKE_COMMAND} -E copy ${SHADER} ${SHADER_BUILD_DIR}/${SHADER_NAME}
        COMMAND ${CMAKE_COMMAND} -E copy ${SHADER} ${SHADER_DEBUG_DIR}/${SHADER_NAME}
        DEPENDS ${SHADER}
        COMMENT "Copying shader source ${SHADER_NAME}"
    )

    list(APPEND SHADER_OUTPUTS
        ${SHADER_BUILD_DIR}/${SHADER_NAME}
        ${SHADER_BUILD_DIR}/${SHADER_NAME}.spv
        ${SHADER_DEBUG_DIR}/${SHADER_NAME}
        ${SHADER_DEBUG_DIR}/${SHADER_NAME}.spv
    )
    set(SHADER_OUTPUTS ${SHADER_OUTPUTS} PARENT_SCOPE)
endfunction()

# Process all shaders
set(SHADER_OUTPUTS "")
foreach(SHADER ${SHADER_SOURCES})
    compile_and_copy_shader(${SHADER})
endforeach()

# Create shader target that main target will depend on
add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})
add_dependencies(${PROJECT_NAME} shaders)

# Print configuration summary
message(STATUS "Configuration Summary")
message(STATUS "-------------------")
message(STATUS "Vulkan SDK Path: ${VULKAN_SDK}")
message(STATUS "Vulkan Include Dir: ${VULKAN_INCLUDE_DIR}")
message(STATUS "Vulkan Lib Dir: ${VULKAN_LIB_DIR}")
message(STATUS "GLFW Path: ${CMAKE_CURRENT_SOURCE_DIR}/external/glfw")
message(STATUS "GLM Path: ${CMAKE_CURRENT_SOURCE_DIR}/external/glm")
message(STATUS "Shader Directory: ${CMAKE_CURRENT_SOURCE_DIR}/shaders")

# Handle compile_commands.json
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    # Function to copy compile_commands.json with error handling
    function(copy_compile_commands)
        if(EXISTS "${CMAKE_BINARY_DIR}/compile_commands.json")
            file(COPY "${CMAKE_BINARY_DIR}/compile_commands.json"
                DESTINATION "${CMAKE_SOURCE_DIR}"
                RESULT variable
                ERROR_VARIABLE error
            )
            if(error)
                message(WARNING "Failed to copy compile_commands.json: ${error}")
            else()
                message(STATUS "Successfully copied compile_commands.json to source directory")
            endif()
        else()
            message(STATUS "compile_commands.json not found in build directory")
        endif()
    endfunction()

    if(MSVC)
        # For Visual Studio, add an optional target
        add_custom_target(
            create_compile_commands
            COMMAND ${CMAKE_COMMAND} -P
                ${CMAKE_BINARY_DIR}/copy_compile_commands.cmake
            COMMENT "Attempting to copy compile_commands.json to source directory"
        )

        # Generate a CMake script to handle the copy
        file(WRITE
            ${CMAKE_BINARY_DIR}/copy_compile_commands.cmake
            "include(\"${CMAKE_SOURCE_DIR}/CMakeLists.txt\")\ncopy_compile_commands()"
        )
    else()
        # For other compilers, use post-build command but don't make it critical
        add_custom_command(
            TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E cmake_echo_color --cyan
                "Attempting to copy compile_commands.json..."
            COMMAND ${CMAKE_COMMAND} -P
                ${CMAKE_BINARY_DIR}/copy_compile_commands.cmake
            VERBATIM
        )

        # Generate the copy script
        file(WRITE
            ${CMAKE_BINARY_DIR}/copy_compile_commands.cmake
            "include(\"${CMAKE_SOURCE_DIR}/CMakeLists.txt\")\ncopy_compile_commands()"
        )
    endif()
endif()