# Convertidor DirectX .X a FBX

Convertidor profesional de archivos DirectX 9 (.x) a formato FBX con soporte completo para:
- ✅ Meshes (geometría)
- ✅ Texturas y Materiales
- ✅ Skeleton (jerarquía de huesos)
- ✅ Skin Weights (pesos de vértices)
- ✅ Animaciones (keyframes)
- ✅ Conversión de matrices Left-Handed → Right-Handed

## Requisitos

### SDK Necesarios
1. **FBX SDK 2020.3.4** (64-bit)
   - Descargar de: https://www.autodesk.com/developer-network/platform-technologies/fbx-sdk-2020-3-4
   - Instalar en: `C:\Program Files\Autodesk\FBX\FBX SDK\2020.3.4\`

2. **DirectX SDK** (June 2010)
   - Descargar de: https://www.microsoft.com/en-us/download/details.aspx?id=6812
   - Instalar en: `C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\`

3. **Visual Studio 2019 o 2022** (64-bit)
   - Con C++ Desktop Development workload

### Opcional: 3ds Max SDK
Si deseas compilar como plugin de 3ds Max:
- **3ds Max SDK 2024** (64-bit)
- Descargar de: https://www.autodesk.com/developer-network/platform-technologies/3ds-max

## Estructura del Proyecto

```
XtoFBX_Converter/
├── src/
│   ├── main.cpp                    # Punto de entrada
│   ├── XFileParser.h/cpp           # Parser de archivos .X
│   ├── FBXExporter.h/cpp           # Exportador FBX
│   ├── MatrixConverter.h/cpp       # Conversión de matrices
│   ├── SkeletonBuilder.h/cpp       # Constructor de skeleton
│   ├── SkinWeightsExporter.h/cpp   # Exportador de skin weights
│   └── AnimationExporter.h/cpp     # Exportador de animaciones
├── include/
│   └── Common.h                    # Definiciones comunes
├── build/                          # Archivos de compilación
└── bin/                           # Ejecutables generados
```

## Compilación

### Con CMake (Recomendado)

```bash
cd XtoFBX_Converter
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### Con Visual Studio (Manual)

1. Abrir Visual Studio 2019/2022
2. Crear nuevo proyecto: **Console App (C++)**
3. Configurar propiedades del proyecto:

**Incluir Directorios:**
- `C:\Program Files\Autodesk\FBX\FBX SDK\2020.3.4\include`
- `C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include`

**Librería Directorios:**
- `C:\Program Files\Autodesk\FBX\FBX SDK\2020.3.4\lib\vs2019\x64\release`
- `C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Lib\x64`

**Librerías Adicionales:**
- `libfbxsdk-md.lib`
- `d3dx9.lib`
- `d3d9.lib`

4. Compilar en modo **Release x64**

## Uso

### Modo Consola

```bash
# Convertir un solo archivo
XtoFBXConverter.exe input.x output.fbx

# Convertir con opciones
XtoFBXConverter.exe input.x output.fbx --version fbx2020 --up-axis Y --scale 1.0

# Batch conversion
XtoFBXConverter.exe --batch input_folder/ output_folder/ --recursive
```

### Opciones Avanzadas

```bash
--fbx-version [2020|2019|2018]     # Versión de FBX (default: 2020)
--up-axis [Y|Z]                    # Eje vertical (default: Y)
--front-axis [X|Y|Z]               # Eje frontal (default: Z)
--coordinate-system [RH|LH]        # Right/Left handed (default: RH)
--scale <float>                    # Factor de escala (default: 1.0)
--merge-materials                  # Fusionar materiales duplicados
--triangulate                      # Triangular polígonos
--export-textures                  # Copiar texturas al directorio de salida
--texture-format [TGA|PNG|JPG]     # Convertir texturas
--verbose                          # Mostrar información detallada
```

## Ejemplos

### Ejemplo 1: Conversión básica
```bash
XtoFBXConverter.exe tiny.x tiny.fbx
```

### Ejemplo 2: Para Unity (Right-Handed, Y-Up)
```bash
XtoFBXConverter.exe character.x character.fbx --up-axis Y --coordinate-system RH --scale 0.01
```

### Ejemplo 3: Para Unreal Engine (Z-Up)
```bash
XtoFBXConverter.exe model.x model.fbx --up-axis Z --scale 100.0
```

### Ejemplo 4: Con texturas
```bash
XtoFBXConverter.exe skinned_mesh.x output.fbx --export-textures --texture-format PNG
```

## Características Técnicas

### Conversión de Matrices
El convertidor maneja automáticamente la conversión de sistemas de coordenadas:

**DirectX (Left-Handed):**
```
X = Right
Y = Up
Z = Forward
```

**FBX (Right-Handed por defecto):**
```
X = Right
Y = Up
Z = Backward (invertido)
```

**Conversión aplicada:**
```cpp
// Para vectores de posición
fbx_position.x =  dx_position.x;
fbx_position.y =  dx_position.y;
fbx_position.z = -dx_position.z;  // Invertir Z

// Para matrices 4x4
fbx_matrix = ConvertMatrix_LH_to_RH(dx_matrix);
```

### Formato .X Soportado
- **Versión:** DirectX 9.0c
- **Formato:** Binario y Texto
- **Templates soportados:**
  - Frame (jerarquía)
  - Mesh (geometría)
  - Material / TextureFilename
  - SkinWeights (influencias de huesos)
  - AnimationSet / Animation

## Troubleshooting

### Error: "Cannot find FBX SDK"
- Verificar que FBX SDK esté instalado en la ruta correcta
- Actualizar las variables de entorno si es necesario

### Error: "d3dx9.dll not found"
- Instalar DirectX End-User Runtime
- Copiar d3dx9_43.dll al directorio del ejecutable

### Texturas no se exportan
- Verificar que las rutas de texturas en el .x sean relativas
- Usar la opción `--export-textures` para copiar archivos

### Animaciones deformadas
- Revisar la orientación del skeleton
- Ajustar `--coordinate-system` y `--up-axis`

## Limitaciones Conocidas

- Archivos .x con HLSL effects no son soportados
- Morphing targets no implementado (solo skeleton animation)
- Máximo 4 influencias de huesos por vértice (estándar)

## Licencia

Este proyecto es código de ejemplo educativo.
FBX® es marca registrada de Autodesk, Inc.
DirectX® es marca registrada de Microsoft Corporation.

## Contacto

Para reportar bugs o solicitar características, crea un issue en el repositorio.
