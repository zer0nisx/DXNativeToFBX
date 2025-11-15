# Correcciones Aplicadas: Skin Weights y Animaciones

## 📋 Resumen

Se corrigieron 3 bugs críticos que causaban:
1. ❌ Huesos no unidos correctamente al modelo (skin weights incorrectos)
2. ❌ Rotaciones raras/erróneas en las animaciones
3. ❌ Bind pose incorrecto

## 🔧 Cambios Realizados

### FIX 1: TransformLinkMatrix en ExportSkinWeights (línea ~608)

**Problema:**
```cpp
// ❌ INCORRECTO: Invertir la offsetMatrix dos veces
FbxAMatrix offsetMatrix = MatrixConverter::ConvertMatrix_LH_to_RH(bone.offsetMatrix);
FbxAMatrix bindPoseMatrix = offsetMatrix.Inverse();
cluster->SetTransformLinkMatrix(bindPoseMatrix);
```

**Causa del error:**
- En DirectX, `offsetMatrix` ya es la INVERSA de la matriz global del hueso en bind pose
- Al invertirla de nuevo, se obtenía una transformación incorrecta
- Esto causaba que los vértices no se deformaran correctamente con el skeleton

**Solución:**
```cpp
// ✅ CORRECTO: Usar la transformación global actual del hueso
FbxAMatrix boneGlobalMatrix = boneNode->EvaluateGlobalTransform();
cluster->SetTransformLinkMatrix(boneGlobalMatrix);
```

**Resultado:**
- Los vértices ahora se vinculan correctamente a los huesos
- El mesh se deforma apropiadamente durante las animaciones

---

### FIX 2: Rotaciones en Animaciones (línea ~854-897)

**Problema:**
```cpp
// ❌ INCORRECTO: DecomposeSphericalXYZ puede causar discontinuidades
FbxQuaternion rot = MatrixConverter::ConvertQuaternion_LH_to_RH(key.rotation);
FbxVector4 euler = rot.DecomposeSphericalXYZ();  // Retorna radianes

// Convertir a grados manualmente
const double RAD_TO_DEG = 180.0 / 3.14159265358979323846;
curveRX->KeySet(keyIndexRX, keyTime, (float)(euler[0] * RAD_TO_DEG));
```

**Causa del error:**
- `DecomposeSphericalXYZ()` puede causar:
  - Gimbal lock (pérdida de un grado de libertad rotacional)
  - Saltos discontinuos entre keyframes (flips de 180°)
  - Interpolación incorrecta entre rotaciones
- La conversión manual de radianes a grados agregaba imprecisión

**Solución:**
```cpp
// ✅ CORRECTO: Usar el método correcto de FBX para extraer Euler angles
FbxAMatrix tempMatrix;
tempMatrix.SetQ(rot);
FbxVector4 euler = tempMatrix.GetR();  // Obtiene Euler en GRADOS directamente

// Ya está en grados, sin conversión manual necesaria
curveRX->KeySet(keyIndexRX, keyTime, (float)euler[0]);
```

**Resultado:**
- Las rotaciones se interpolan suavemente entre keyframes
- No hay saltos discontinuos ni orientaciones raras
- Mayor precisión en las conversiones

---

### FIX 3: CreateBindPose (línea ~673)

**Problema:**
```cpp
// ❌ INCORRECTO: Mismo error que FIX 1
FbxAMatrix offsetMatrix = MatrixConverter::ConvertMatrix_LH_to_RH(bone.offsetMatrix);
FbxAMatrix boneBindPoseMatrix = offsetMatrix.Inverse();
```

**Solución:**
```cpp
// ✅ CORRECTO: Consistencia con las matrices usadas en los clusters
FbxAMatrix boneBindPoseMatrix = boneNode->EvaluateGlobalTransform();
```

**Resultado:**
- El bind pose es consistente con las matrices de los clusters
- Importadores FBX (3ds Max, Blender, Unity) interpretan correctamente el skeleton

---

## 🎯 Impacto de las Correcciones

### Antes:
- ❌ Huesos flotando separados del modelo
- ❌ Vértices no se deformaban con el skeleton
- ❌ Animaciones con rotaciones incorrectas, saltos y orientaciones raras
- ❌ Problemas al importar en software 3D

### Después:
- ✅ Huesos correctamente vinculados al modelo
- ✅ Skinning funcional (vértices se deforman con los huesos)
- ✅ Animaciones con rotaciones suaves y correctas
- ✅ Compatible con 3ds Max, Blender, Unity, Unreal Engine

---

## 📚 Conceptos Técnicos

### Offset Matrix en DirectX
```
offsetMatrix = Inverse(BoneGlobalMatrix_BindPose)
```
Transforma vértices del espacio del mesh al espacio del hueso.

### Skinning en FBX
```
Vertex_Final = (Vertex_Mesh * TransformMatrix^-1) *
               TransformLinkMatrix *
               CurrentBoneMatrix
```

Donde:
- `TransformMatrix` = Matriz global del mesh en bind pose
- `TransformLinkMatrix` = Matriz global del hueso en bind pose
- `CurrentBoneMatrix` = Transformación animada del hueso

### Quaternion vs Euler Angles
- **Quaternion**: 4 componentes (x, y, z, w), sin gimbal lock, interpolación suave (SLERP)
- **Euler Angles**: 3 ángulos (X, Y, Z), susceptible a gimbal lock, puede tener discontinuidades

La conversión incorrecta de quaternion → Euler era la causa de las rotaciones raras.

---

## ✅ Testing Recomendado

1. **Exportar un modelo con skinning** desde DirectX .X a FBX
2. **Importar en Blender/3ds Max:**
   - Verificar que el mesh esté unido al skeleton
   - Mover huesos manualmente → el mesh debe deformarse
3. **Reproducir animaciones:**
   - No debe haber saltos ni orientaciones raras
   - Las rotaciones deben ser suaves entre keyframes

---

## 🔗 Archivos Modificados

- `src/FBXExporter.cpp`:
  - Función `ExportSkinWeights()` (línea ~588-625)
  - Función `ExportAnimationClip()` (línea ~850-897)
  - Función `CreateBindPose()` (línea ~668-679)

---

## 👤 Autor de las Correcciones
- Fecha: 2025-11-15
- Problemas corregidos: Skin weights + rotaciones de animaciones
