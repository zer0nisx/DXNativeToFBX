# Tareas - Corrección de Exportación FBX

## ✅ Problemas Críticos Corregidos

- [x] **FIX 1**: Corregir cálculo de TransformLinkMatrix en ExportSkinWeights (línea 608)
  - Usar `boneNode->EvaluateGlobalTransform()` en lugar de invertir offsetMatrix

- [x] **FIX 2**: Corregir exportación de rotaciones en animaciones (línea 854-858)
  - Usar matriz temporal con SetQ() y extraer Euler con GetR() para mantener continuidad

- [x] **FIX 3**: Corregir CreateBindPose (línea 673)
  - Usar transformación global del hueso correctamente

## 📋 Pendientes

- [ ] Hacer commit con las correcciones
- [ ] Push de los cambios al repositorio
