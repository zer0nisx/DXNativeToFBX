cmake_minimum_required(VERSION 3.15)

project(XtoFBXConverter VERSION 1.0.0 LANGUAGES CXX)

# C++17 requerido
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configuración para Windows x64
if(WIN32)
    set(CMAKE_GENERATOR_PLATFORM x64)
endif()

# =============================================================================
# Rutas de SDKs (ACTUALIZADAS CON TUS RUTAS EXACTAS)
# =============================================================================

# FBX SDK - Rutas exactas
set(FBX_SDK_ROOT "C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.7" CACHE PATH "FBX SDK root directory" FORCE)
set(FBX_SDK_INCLUDE "${FBX_SDK_ROOT}/include")
set(FBX_SDK_LIB_DIR "${FBX_SDK_ROOT}/lib/x64/release")

# DirectX SDK (June 2010) - Rutas exactas
set(DIRECTX_SDK_INCLUDE "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)/Include")
set(DIRECTX_SDK_LIB_DIR "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)/Lib/x64")

# 3ds Max 2012 SDK - Rutas exactas (no se usa actualmente, pero disponibles)
set(MAX_SDK_INCLUDE "C:/Program Files (x86)/Autodesk/3ds Max 2012/maxsdk/include")
set(MAX_SDK_LIB_DIR "C:/Program Files (x86)/Autodesk/3ds Max 2012/maxsdk/x64/lib")


# =============================================================================
# Verificar que los SDKs existen
# =============================================================================

if(NOT EXISTS ${DIRECTX_SDK_INCLUDE})
    message(WARNING "DirectX SDK not found at: ${DIRECTX_SDK_INCLUDE}")
else()
    message(STATUS "DirectX SDK found: ${DIRECTX_SDK_INCLUDE}")
endif()

# Verificar 3ds Max SDK (opcional, solo informativo)
if(NOT EXISTS ${MAX_SDK_INCLUDE})
    message(STATUS "3ds Max SDK not found at: ${MAX_SDK_INCLUDE}")
else()
    message(STATUS "3ds Max 2012 SDK found: ${MAX_SDK_INCLUDE}")
endif()


# =============================================================================
# Archivos fuente COMUNES
# =============================================================================

set(COMMON_SOURCES
    src/XFileParser.cpp
    src/FBXExporter.cpp
    src/MatrixConverter.cpp
)

set(COMMON_HEADERS
    include/Common.h
    src/XFileParser.h
    src/FBXExporter.h
    src/MatrixConverter.h
)

# =============================================================================
# 1. EJECUTABLE STANDALONE (Consola)
# =============================================================================

add_executable(XtoFBXConverter
    src/main.cpp
    ${COMMON_SOURCES}
    ${COMMON_HEADERS}
)

target_include_directories(XtoFBXConverter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FBX_SDK_INCLUDE}
    ${DIRECTX_SDK_INCLUDE}
)

# Buscar librería FBX - Ruta exacta
set(FBX_LIB_NAME "libfbxsdk-md.lib")

# Buscar directamente en la ruta exacta proporcionada
find_library(FBX_LIBRARY
    NAMES libfbxsdk-md libfbxsdk libfbxsdk-md.lib libfbxsdk.lib
    PATHS "C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.7/lib/x64/release"
          "${FBX_SDK_LIB_DIR}"
          "${FBX_SDK_ROOT}/lib/x64/release"
    NO_DEFAULT_PATH
)

if(NOT FBX_LIBRARY)
    message(WARNING "FBX SDK library not found in exact path. Trying broader search...")
    find_library(FBX_LIBRARY
        NAMES libfbxsdk-md libfbxsdk libfbxsdk-md.lib libfbxsdk.lib
        PATHS "${FBX_SDK_ROOT}"
        PATH_SUFFIXES "lib/x64/release" "lib/x64/debug" "lib/x64" "lib"
    )
endif()

if(FBX_LIBRARY)
    message(STATUS "FBX Library found: ${FBX_LIBRARY}")
else()
    message(WARNING "FBX Library NOT FOUND at: C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.7/lib/x64/release")
endif()

# DirectX libraries
find_library(D3D9_LIBRARY
    NAMES d3d9
    PATHS ${DIRECTX_SDK_LIB_DIR}
    REQUIRED
)

find_library(D3DX9_LIBRARY
    NAMES d3dx9
    PATHS ${DIRECTX_SDK_LIB_DIR}
    REQUIRED
)

target_link_libraries(XtoFBXConverter PRIVATE
    ${FBX_LIBRARY}
    ${D3D9_LIBRARY}
    ${D3DX9_LIBRARY}
    # FBX SDK dependencies (XML2 and ZLib)
    # These may need to be installed separately or come with FBX SDK
    ws2_32.lib
    winmm.lib
)

# FBX SDK requires libxml2 and zlib
# These can be installed via vcpkg or downloaded separately
# Try to find them in common locations

# Try vcpkg first (if available)
if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT $ENV{VCPKG_ROOT})
    find_library(XML2_LIBRARY
        NAMES xml2 libxml2
        PATHS "${VCPKG_ROOT}/installed/x64-windows/lib"
        NO_DEFAULT_PATH
    )
    find_library(ZLIB_LIBRARY
        NAMES zlib zlibd z
        PATHS "${VCPKG_ROOT}/installed/x64-windows/lib"
        NO_DEFAULT_PATH
    )
endif()

# Try standard locations
if(NOT XML2_LIBRARY)
    find_library(XML2_LIBRARY
        NAMES xml2 libxml2
        PATHS "${FBX_SDK_ROOT}/lib/x64/release"
              "${FBX_SDK_ROOT}/lib"
              "C:/Program Files/libxml2/lib"
              "C:/vcpkg/installed/x64-windows/lib"
    )
endif()

if(NOT ZLIB_LIBRARY)
    find_library(ZLIB_LIBRARY
        NAMES zlib zlibd z
        PATHS "${FBX_SDK_ROOT}/lib/x64/release"
              "${FBX_SDK_ROOT}/lib"
              "C:/Program Files/zlib/lib"
              "C:/vcpkg/installed/x64-windows/lib"
    )
endif()

if(XML2_LIBRARY)
    target_link_libraries(XtoFBXConverter PRIVATE ${XML2_LIBRARY})
    message(STATUS "XML2 library found: ${XML2_LIBRARY}")
else()
    message(FATAL_ERROR "XML2 library not found. Please install libxml2.")
    message(STATUS "You can install it using vcpkg: vcpkg install libxml2:x64-windows")
endif()

if(ZLIB_LIBRARY)
    target_link_libraries(XtoFBXConverter PRIVATE ${ZLIB_LIBRARY})
    message(STATUS "ZLib library found: ${ZLIB_LIBRARY}")
else()
    message(FATAL_ERROR "ZLib library not found. Please install zlib.")
    message(STATUS "You can install it using vcpkg: vcpkg install zlib:x64-windows")
endif()

target_compile_definitions(XtoFBXConverter PRIVATE
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    FBXSDK_SHARED
)

if(MSVC)
    target_compile_options(XtoFBXConverter PRIVATE
        /W3 /MP /EHsc /permissive-
    )
    if(CMAKE_BUILD_TYPE MATCHES Release)
        target_compile_options(XtoFBXConverter PRIVATE /O2 /Ot)
    endif()
endif()


# =============================================================================
# Copiar DLLs necesarias
# =============================================================================

# Buscar DLL de FBX - Ruta exacta
set(FBX_DLL "")
set(FBX_DLL_SEARCH_PATHS
    "C:/Program Files/Autodesk/FBX/FBX SDK/2020.3.7/lib/x64/release/libfbxsdk.dll"
    "${FBX_SDK_LIB_DIR}/libfbxsdk.dll"
    "${FBX_SDK_ROOT}/lib/x64/release/libfbxsdk.dll"
    "${FBX_SDK_ROOT}/lib/x64/debug/libfbxsdk.dll"
)

foreach(SEARCH_PATH ${FBX_DLL_SEARCH_PATHS})
    if(EXISTS ${SEARCH_PATH})
        set(FBX_DLL ${SEARCH_PATH})
        break()
    endif()
endforeach()

if(EXISTS ${FBX_DLL})
    add_custom_command(TARGET XtoFBXConverter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FBX_DLL}
        $<TARGET_FILE_DIR:XtoFBXConverter>
        COMMENT "Copying FBX SDK DLL..."
    )
else()
    message(WARNING "FBX SDK DLL not found. The executable may not run without it.")
endif()

# =============================================================================
# Instalación
# =============================================================================

install(TARGETS XtoFBXConverter
    RUNTIME DESTINATION bin
)

# =============================================================================
# Resumen de configuración
# =============================================================================

message(STATUS "=============================================================================")
message(STATUS "  X to FBX Converter - Configuration Summary")
message(STATUS "=============================================================================")
message(STATUS "  CMake Version:        ${CMAKE_VERSION}")
message(STATUS "  Build Type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:         C++${CMAKE_CXX_STANDARD}")
message(STATUS "  ")
if(FBX_SDK_ROOT)
    message(STATUS "  FBX SDK Root:         ${FBX_SDK_ROOT}")
    if(FBX_LIBRARY)
        message(STATUS "  FBX Library:          ${FBX_LIBRARY}")
    else()
        message(WARNING "  FBX Library:          NOT FOUND")
    endif()
else()
    message(WARNING "  FBX SDK Root:         NOT CONFIGURED")
    message(WARNING "  FBX Library:          NOT FOUND")
    message(STATUS "  ")
    message(STATUS "  To configure FBX SDK, run cmake with:")
    message(STATUS "    cmake -DFBX_SDK_ROOT=<path_to_fbx_sdk> ..")
endif()
message(STATUS "  ")
message(STATUS "  DirectX SDK Include:  ${DIRECTX_SDK_INCLUDE}")
message(STATUS "  DirectX SDK Lib:     ${DIRECTX_SDK_LIB_DIR}")
message(STATUS "  D3D9 Library:         ${D3D9_LIBRARY}")
message(STATUS "  D3DX9 Library:        ${D3DX9_LIBRARY}")
message(STATUS "  ")
if(EXISTS ${MAX_SDK_INCLUDE})
    message(STATUS "  3ds Max SDK Include:  ${MAX_SDK_INCLUDE}")
    message(STATUS "  3ds Max SDK Lib:     ${MAX_SDK_LIB_DIR}")
endif()
message(STATUS "=============================================================================")
