#pragma once

#ifndef FBX_EXPORTER_H
#define FBX_EXPORTER_H

#include "../include/Common.h"
#include "MatrixConverter.h"

/**
 * @class FBXExporter
 * @brief Exporta datos de escena a formato FBX
 *
 * Maneja la exportación completa de:
 * - Geometría (meshes)
 * - Materiales y texturas
 * - Skeleton (jerarquía de huesos)
 * - Skin weights (deformación)
 * - Animaciones (keyframes)
 */
class FBXExporter
{
public:
    FBXExporter();
    ~FBXExporter();

    /**
     * Exportar escena completa a archivo FBX
     * @param sceneData Datos de la escena a exportar
     * @param filename Archivo FBX de salida
     * @param options Opciones de conversión
     * @return true si se exportó exitosamente
     */
    bool ExportScene(
        const SceneData& sceneData,
        const string& filename,
        const ConversionOptions& options);

    /**
     * Exportar una animación individual a un archivo FBX separado
     * @param sceneData Datos de la escena (sin animaciones)
     * @param animation Clip de animación a exportar
     * @param filename Archivo FBX de salida
     * @param options Opciones de conversión
     * @return true si se exportó exitosamente
     */
    bool ExportSingleAnimation(
        const SceneData& sceneData,
        const AnimationClip& animation,
        const string& filename,
        const ConversionOptions& options);

    /**
     * Obtener último mensaje de error
     * @return Mensaje de error
     */
    string GetLastError() const { return m_LastError; }

private:
    // FBX SDK Manager y Scene
    FbxManager* m_pManager;
    FbxScene* m_pScene;

    // Opciones actuales
    ConversionOptions m_Options;

    // Mapeo de nombres de huesos a FbxNode* (para animaciones)
    map<string, FbxNode*> m_BoneNodeMap;

    // Último error
    string m_LastError;

    /**
     * Inicializar FBX SDK
     * @return true si se inicializó correctamente
     */
    bool Initialize();

    /**
     * Liberar recursos FBX
     */
    void Shutdown();

    /**
     * Crear escena FBX desde SceneData
     * @param sceneData Datos a exportar
     * @return true si se creó exitosamente
     */
    bool CreateFBXScene(const SceneData& sceneData);

    /**
     * Exportar jerarquía de frames
     * @param frameData Frame a exportar
     * @param parentNode Nodo padre en FBX
     * @return FbxNode* creado
     */
    FbxNode* ExportFrame(FrameData* frameData, FbxNode* parentNode);

    /**
     * Exportar mesh
     * @param meshData Mesh a exportar
     * @param frameNode Nodo del frame
     * @return FbxNode* con el mesh
     */
    FbxNode* ExportMesh(
        MeshData* meshData,
        FbxNode* frameNode,
        const vector<MaterialData>& materials);

    /**
     * Exportar geometría del mesh
     * @param meshData Datos del mesh
     * @param fbxMesh Mesh FBX donde exportar
     */
    void ExportGeometry(MeshData* meshData, FbxMesh* fbxMesh);

    /**
     * Exportar UVs del mesh
     * @param meshData Datos del mesh
     * @param fbxMesh Mesh FBX
     */
    void ExportUVs(MeshData* meshData, FbxMesh* fbxMesh);

    /**
     * Exportar normales del mesh
     * @param meshData Datos del mesh
     * @param fbxMesh Mesh FBX
     */
    void ExportNormals(MeshData* meshData, FbxMesh* fbxMesh);

    /**
     * Exportar materiales
     * @param materials Lista de materiales
     * @param meshNode Nodo del mesh
     * @param meshData Datos del mesh
     */
    void ExportMaterials(
        const vector<MaterialData>& materials,
        FbxNode* meshNode,
        MeshData* meshData);

    /**
     * Crear material FBX desde MaterialData
     * @param matData Datos del material
     * @return FbxSurfacePhong* creado
     */
    FbxSurfacePhong* CreateMaterial(const MaterialData& matData);

    /**
     * Exportar skeleton (jerarquía de huesos)
     * @param meshData Mesh con información de skinning
     * @param meshNode Nodo del mesh
     * @param rootFrame Frame raíz de la escena
     */
    void ExportSkeleton(
        MeshData* meshData,
        FbxNode* meshNode,
        FrameData* rootFrame);

    /**
     * Crear hueso (skeleton node)
     * @param boneName Nombre del hueso
     * @param transformMatrix Matriz de transformación
     * @param parentNode Nodo padre
     * @return FbxNode* del hueso
     */
    FbxNode* CreateBone(
        const string& boneName,
        const D3DXMATRIX& transformMatrix,
        FbxNode* parentNode);

    /**
     * Exportar skin weights (deformación)
     * @param meshData Mesh con pesos
     * @param fbxMesh Mesh FBX
     * @param meshNode Nodo del mesh
     */
    void ExportSkinWeights(
        MeshData* meshData,
        FbxMesh* fbxMesh,
        FbxNode* meshNode);

    /**
     * Exportar animaciones
     * @param animations Lista de animaciones
     */
    void ExportAnimations(const vector<AnimationClip>& animations);

    /**
     * Exportar un clip de animación
     * @param clip Clip a exportar
     */
    void ExportAnimationClip(const AnimationClip& clip);

    /**
     * Configurar propiedades globales de la escena
     */
    void SetupSceneProperties();

    /**
     * Configurar sistema de coordenadas FBX
     */
    void SetupCoordinateSystem();

    /**
     * Buscar frame por nombre (recursivo)
     * @param root Frame raíz
     * @param name Nombre a buscar
     * @return FrameData* encontrado o nullptr
     */
    FrameData* FindFrameByName(FrameData* root, const string& name);

    /**
     * Copiar texturas al directorio de salida
     * @param textureFilename Ruta de la textura original
     * @return Ruta de la textura copiada
     */
    string CopyTexture(const string& textureFilename);
};

#endif // FBX_EXPORTER_H
