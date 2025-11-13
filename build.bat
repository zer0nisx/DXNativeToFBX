@echo off
REM =============================================================================
REM Script de compilación para X to FBX Converter
REM =============================================================================

echo.
echo =============================================================================
echo   X to FBX Converter - Build Script
echo =============================================================================
echo.

REM Verificar que Visual Studio está instalado
where cmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake not found in PATH
    echo Please install CMake and add it to PATH
    pause
    exit /b 1
)

REM Crear directorio de compilación
if not exist build (
    mkdir build
    echo Created build directory
)

cd build

echo.
echo Step 1: Configuring with CMake...
echo.

REM Configurar proyecto con CMake
cmake .. -G "Visual Studio 16 2019" -A x64

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: CMake configuration failed
    echo.
    echo Common issues:
    echo   - FBX SDK not installed or wrong path
    echo   - DirectX SDK not installed or wrong path
    echo   - Wrong Visual Studio version
    echo.
    echo Please check:
    echo   1. FBX SDK 2020.3.4 is installed at: C:\Program Files\Autodesk\FBX\FBX SDK\2020.3.4
    echo   2. DirectX SDK June 2010 is installed at: C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)
    echo   3. Visual Studio 2019 is installed
    echo.
    pause
    exit /b 1
)

echo.
echo Step 2: Building Release version...
echo.

REM Compilar en Release
cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
echo =============================================================================
echo   BUILD SUCCESSFUL!
echo =============================================================================
echo.
echo Executable location: build\Release\XtoFBXConverter.exe
echo.
echo To test the converter, run:
echo   Release\XtoFBXConverter.exe input.x output.fbx
echo.
echo For help:
echo   Release\XtoFBXConverter.exe --help
echo.
echo =============================================================================
echo.

cd ..

REM Copiar ejecutable al directorio bin
if not exist bin (
    mkdir bin
)

copy build\Release\XtoFBXConverter.exe bin\ >nul 2>&1
copy build\Release\*.dll bin\ >nul 2>&1

echo Executable copied to: bin\XtoFBXConverter.exe
echo.

pause
