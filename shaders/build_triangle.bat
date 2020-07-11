@echo off
if "%VULKAN_SDK%"=="" echo Error! Vulkan SDK not found!
echo Using Vulkan SDK at %VULKAN_SDK%
set "_GLSLC_PATH=%VULKAN_SDK%\Bin\glslc.exe"
echo Using GLSLC at %_GLSLC_PATH%
%_GLSLC_PATH% --target-env=vulkan1.0 -c triangle.vert
%_GLSLC_PATH% --target-env=vulkan1.0 -c triangle.frag