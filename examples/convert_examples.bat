@echo off
REM =============================================================================
REM Ejemplos de conversión de .X a FBX
REM =============================================================================

echo.
echo =============================================================================
echo   X to FBX Converter - Conversion Examples
echo =============================================================================
echo.

set CONVERTER=..\bin\XtoFBXConverter.exe

if not exist %CONVERTER% (
    echo ERROR: Converter not found at: %CONVERTER%
    echo Please build the project first using build.bat
    pause
    exit /b 1
)

REM Crear directorio de salida
if not exist output (
    mkdir output
)

echo.
echo Example 1: Basic conversion
echo -------------------------------------------------------
%CONVERTER% sample.x output\sample.fbx

echo.
echo.
echo Example 2: Conversion for Unity (Right-Handed, Y-Up)
echo -------------------------------------------------------
%CONVERTER% character.x output\character_unity.fbx --up-axis Y --coordinate-system RH --scale 0.01

echo.
echo.
echo Example 3: Conversion for Unreal Engine (Z-Up, scaled)
echo -------------------------------------------------------
%CONVERTER% model.x output\model_unreal.fbx --up-axis Z --scale 100.0

echo.
echo.
echo Example 4: Conversion with texture export
echo -------------------------------------------------------
%CONVERTER% skinned_mesh.x output\skinned_mesh.fbx --export-textures --verbose

echo.
echo.
echo Example 5: Conversion for 3ds Max (Z-Up)
echo -------------------------------------------------------
%CONVERTER% environment.x output\environment_max.fbx --up-axis Z --coordinate-system RH

echo.
echo.
echo Example 6: Conversion for Maya (Y-Up, default)
echo -------------------------------------------------------
%CONVERTER% animation.x output\animation_maya.fbx --fbx-version 2020

echo.
echo =============================================================================
echo   All examples completed!
echo =============================================================================
echo.
echo Output files are in: output\
echo.
pause
