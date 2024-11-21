@echo off
setlocal enabledelayedexpansion

REM Change to the source folder
cd resources\shaders\vulkan

REM Loop through all .vert and .frag files in the folder
for %%f in (*.vert *.frag *.rgen *.rmiss *.rchit) do (
    REM Compile shaders to .spv
    "%VK_SDK_PATH%\Bin\glslc.exe" "%%f" -o "%%f.spv" --target-env=vulkan1.3 --target-spv=spv1.6
)

cd ../../../

echo All shaders compiled successfully^^!