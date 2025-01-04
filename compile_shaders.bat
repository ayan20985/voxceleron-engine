@echo off
echo Compiling shaders...

%VULKAN_SDK%\Bin\glslc.exe shaders/basic.vert -o shaders/basic.vert.spv
%VULKAN_SDK%\Bin\glslc.exe shaders/basic.frag -o shaders/basic.frag.spv

echo Shader compilation complete.
pause 