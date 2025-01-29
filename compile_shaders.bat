@echo off
echo Compiling shaders...

REM Create shaders directory if it doesn't exist
if not exist "shaders" mkdir shaders

REM Compile compute shader
%VULKAN_SDK%\Bin\glslc.exe shaders/mesh_generator.comp -o shaders/mesh_generator.comp.spv

echo Done. 