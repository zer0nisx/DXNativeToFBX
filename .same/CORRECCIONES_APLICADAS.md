# Correcciones Aplicadas al Convertidor DXNativeToFBX

## Fecha: 13 de Noviembre de 2025

## Problemas Resueltos

### 1. **Huesos y Mesh No Están Unidos (Skin Weights)**

**Problema:**
Las animaciones se exportaban correctamente, pero los huesos y la mesh no estaban unidos, causando que la mesh no se deformara con las animaciones.

**Causa Raíz:**
- No se estaba creando el `FbxPose` (bind pose) requerido por FBX
- Las matrices de transformación en los clusters estaban mal configuradas
- La `offsetMatrix` no se estaba convirtiendo correctamente a `bindPoseMatrix`

**Solución Implementada:**

1. **Nueva función `CreateBindPose()`** (`FBXExporter.cpp` línea ~600):
   - Crea un `FbxPose` que almacena las posiciones de todos los huesos y el mesh en el momento del binding
   - Agrega el mesh y todos los huesos al bind pose con sus matrices globales
   - Este paso es CRÍTICO para que programas como Maya, 3ds Max, Unity y Unreal Engine puedan deformar correctamente la mesh

2. **Corrección en `ExportSkinWeights()`** (`FBXExporter.cpp` línea ~585):
   ```cpp
   // ANTES (INCORRECTO):
   cluster->SetTransformLinkMatrix(linkMatrix);  // Matriz actual del hueso

   // DESPUÉS (CORRECTO):
   FbxAMatrix offsetMatrix = MatrixConverter::ConvertMatrix_LH_to_RH(bone.offsetMatrix);
   FbxAMatrix bindPoseMatrix = offsetMatrix.Inverse();  // Invertir la offset matrix
   cluster->SetTransformLinkMatrix(bindPoseMatrix);     // Matriz del hueso en bind pose
   ```

3. **Llamada a `CreateBindPose()`** (`FBXExporter.cpp` línea ~355):
   - Se llama inmediatamente después de `ExportSkinWeights()`
   - Asegura que el bind pose se cree para todos los meshes con skinning

---

### 2. **Errores con la Rotación de Vértices en Animaciones**

**Problema:**
Las rotaciones de los huesos durante las animaciones causaban deformaciones incorrectas en los vértices.

**Causa Raíz:**
Los tiempos de los keyframes estaban en "ticks" (unidades de tiempo internas de DirectX) pero se estaban tratando como segundos directamente, causando:
- Animaciones extremadamente rápidas o lentas
- Interpolación incorrecta entre keyframes
- Rotaciones aplicadas en momentos equivocados

**Solución Implementada:**

1. **Cálculo correcto de `ticksPerSecond`** (`XFileParser.cpp` línea ~537):
   ```cpp
   // ANTES (INCORRECTO):
   clip.ticksPerSecond = pAnimSet->GetPeriodicPosition(1.0) / pAnimSet->GetPeriod();

   // DESPUÉS (CORRECTO):
   double referenceTime = (clip.duration >= 1.0) ? 1.0 : clip.duration;
   double ticksAtReference = pAnimSet->GetPeriodicPosition(referenceTime);
   clip.ticksPerSecond = ticksAtReference / referenceTime;
   ```

2. **Conversión de ticks a segundos en todos los keyframes** (`XFileParser.cpp`):

   **Rotaciones** (línea ~586):
   ```cpp
   key.time = pRotKeys[iKey].Time / clip.ticksPerSecond;  // Convertir ticks → segundos
   ```

   **Traslaciones** (línea ~616):
   ```cpp
   key.time = pPosKeys[iKey].Time / clip.ticksPerSecond;
   ```

   **Escalas** (línea ~676):
   ```cpp
   key.time = pScaleKeys[iKey].Time / clip.ticksPerSecond;
   ```

---

### 3. **Conversión de TPS (Ticks Per Second) a FPS Configurable**

**Problema:**
El formato `.X` maneja el tiempo en TPS (Ticks Per Second), pero FBX y la mayoría de software 3D trabaja con FPS (Frames Per Second). No había forma de configurar el framerate de salida.

**Solución Implementada:**

1. **Nueva opción `targetFPS`** (`Common.h` línea ~73):
   ```cpp
   struct ConversionOptions {
       // ...
       double targetFPS = 30.0;          // FPS objetivo (configurable: 24, 30, 60, etc.)
       bool resampleAnimation = true;    // Opción para futuro resampling
   };
   ```

2. **Configuración del framerate en FBX** (`FBXExporter.cpp` línea ~850):
   ```cpp
   void FBXExporter::SetupSceneProperties() {
       // Determinar el modo de tiempo según el FPS objetivo
       FbxTime::EMode timeMode;
       if (m_Options.targetFPS >= 59.0 && m_Options.targetFPS <= 61.0)
           timeMode = FbxTime::eFrames60;
       else if (m_Options.targetFPS >= 29.0 && m_Options.targetFPS <= 31.0)
           timeMode = FbxTime::eFrames30;
       else if (m_Options.targetFPS >= 23.0 && m_Options.targetFPS <= 25.0)
           timeMode = FbxTime::eFrames24;

       FbxGlobalSettings& globalSettings = m_pScene->GetGlobalSettings();
       globalSettings.SetTimeMode(timeMode);
   }
   ```

3. **Nueva opción de línea de comandos** (`main.cpp` línea ~113):
   ```bash
   --fps <30|60>    # Configurar FPS objetivo para animaciones
   ```

   **Ejemplos de uso:**
   ```bash
   # Exportar a 30 FPS (default)
   XtoFBXConverter.exe character.x character.fbx

   # Exportar a 60 FPS para animaciones suaves
   XtoFBXConverter.exe character.x character.fbx --fps 60

   # Para juegos que usan 24 FPS
   XtoFBXConverter.exe cinematic.x cinematic.fbx --fps 24
   ```

---

## Archivos Modificados

| Archivo | Cambios |
|---------|---------|
| `include/Common.h` | Agregadas opciones `targetFPS` y `resampleAnimation` |
| `src/XFileParser.cpp` | Corrección del cálculo de TPS y conversión de tiempos de ticks a segundos |
| `src/FBXExporter.h` | Agregada declaración de `CreateBindPose()` |
| `src/FBXExporter.cpp` | Implementación de `CreateBindPose()`, corrección de matrices en `ExportSkinWeights()`, configuración de framerate |
| `src/main.cpp` | Agregada opción `--fps` en línea de comandos |
| `README.md` | Documentación actualizada con nueva opción `--fps` |

---

## Validación

Para verificar que las correcciones funcionan:

1. **Verificar skin weights:**
   ```bash
   XtoFBXConverter.exe skinned_character.x output.fbx --verbose
   ```
   - Buscar mensaje: "Bind pose created successfully"
   - Buscar mensaje: "Skin weights exported successfully"

2. **Verificar conversión de tiempo:**
   ```bash
   XtoFBXConverter.exe animated.x output.fbx --verbose
   ```
   - Buscar líneas como: "Animation: Walk, Duration: 2.5s, TPS: 4800"

3. **Verificar FPS:**
   ```bash
   XtoFBXConverter.exe character.x output.fbx --fps 60 --verbose
   ```
   - Buscar mensaje: "FBX Scene framerate set to: 60 FPS"

4. **Importar en software 3D:**
   - Maya: Importar FBX y verificar que la animación funcione correctamente
   - 3ds Max: Verificar que el skinning se mantenga
   - Unity: Importar y verificar que las animaciones se reproduzcan a la velocidad correcta
   - Unreal Engine: Verificar que no haya deformaciones extrañas

---

## Notas Técnicas

### Relación entre TPS y FPS

- **TPS (Ticks Per Second)**: Unidad de tiempo interna de DirectX para animaciones
  - Puede ser cualquier valor (típicamente 4800, 9600, etc.)
  - Más ticks = mayor precisión temporal

- **FPS (Frames Per Second)**: Cuadros por segundo
  - 24 FPS: Estándar de cine
  - 30 FPS: Común en animaciones y videojuegos
  - 60 FPS: Animaciones muy suaves, juegos modernos

- **Conversión**: `tiempo_en_segundos = ticks / ticksPerSecond`

### Matrices de Bind Pose

La relación matemática correcta es:

```
Vertex_Deformed = Vertex_Mesh × TransformMatrix⁻¹ × TransformLinkMatrix × BoneAnimation

Donde:
- TransformMatrix: Matriz global del mesh en bind pose
- TransformLinkMatrix: Matriz global del hueso en bind pose (offsetMatrix⁻¹)
- BoneAnimation: Transformación actual del hueso durante la animación
```

---

## Limitaciones Conocidas

- El resampling de animaciones no está implementado (opción `resampleAnimation` es solo preparatoria)
- Los FPS intermedios (ej: 50 FPS) se redondean al modo más cercano (24, 30, 60)
- Máximo 4 influencias de huesos por vértice (estándar de la industria)

---

## Autor de Correcciones

**AI Assistant** - Same.new Platform
**Fecha:** 13 de Noviembre de 2025
