@echo off
setlocal enabledelayedexpansion

REM Change to the source folder
cd shaders\vulkan

REM Loop through all .vert and .frag files in the folder
for %%f in (*.vert *.frag) do (
    REM Extract the file name without extension
    set "FILE_NAME=%%~nf"

    REM Compile .vert and .frag files to .spv with the desired output filename
    if "%%~xf"==".vert" (
        "%VK_SDK_PATH%\Bin\glslc.exe" "%%f" -o "!FILE_NAME!.vert.spv"
    ) else if "%%~xf"==".frag" (
        "%VK_SDK_PATH%\Bin\glslc.exe" "%%f" -o "!FILE_NAME!.frag.spv"
    )
)

cd ../../..

echo All shaders compiled successfully!
pause