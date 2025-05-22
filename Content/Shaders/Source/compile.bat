:: Requires shadercross CLI installed from SDL_shadercross
@echo off
setlocal enabledelayedexpansion

for %%f in (*.vert.hlsl) do (
    if exist "%%f" (
        shadercross "%%f" -o "..\Compiled\SPIRV\%%~nf.spv"
        shadercross "%%f" -o "..\Compiled\MSL\%%~nf.msl"
        shadercross "%%f" -o "..\Compiled\DXIL\%%~nf.dxil"
    )
)

for %%f in (*.frag.hlsl) do (
    if exist "%%f" (
        shadercross "%%f" -o "..\Compiled\SPIRV\%%~nf.spv"
        shadercross "%%f" -o "..\Compiled\MSL\%%~nf.msl"
        shadercross "%%f" -o "..\Compiled\DXIL\%%~nf.dxil"
    )
)

for %%f in (*.comp.hlsl) do (
    if exist "%%f" (
        shadercross "%%f" -o "..\Compiled\SPIRV\%%~nf.spv"
        shadercross "%%f" -o "..\Compiled\MSL\%%~nf.msl"
        shadercross "%%f" -o "..\Compiled\DXIL\%%~nf.dxil"
    )
)