#pragma once

#ifndef XFILE_PARSER_H
#define XFILE_PARSER_H

#include "../include/Common.h"

/**
 * @class XFileParser
 * @brief Parser para archivos DirectX .X (meshes, animaciones, skinning)
 *
 * Basado en el código de ejemplo de Microsoft DirectX SDK
 * Usa D3DXLoadMeshHierarchyFromX para cargar la jerarquía completa
 */
class XFileParser
{
public:
    XFileParser();
    ~XFileParser();

    /**
     * Cargar archivo .X y parsear todo su contenido
     * @param filename Ruta del archivo .X
     * @param sceneData [out] Datos de la escena parseada
     * @param options Opciones de conversión
     * @return true si se cargó exitosamente
     */
    bool LoadFile(const string& filename, SceneData& sceneData, const ConversionOptions& options);

    /**
     * Obtener información del archivo sin cargarlo completamente
     * @param filename Ruta del archivo
     * @param numMeshes [out] Número de meshes
     * @param numBones [out] Número de huesos
     * @param numAnimations [out] Número de animaciones
     * @return true si se obtuvo la info exitosamente
     */
    bool GetFileInfo(const string& filename, int& numMeshes, int& numBones, int& numAnimations);

private:
    // Device DirectX 9 (necesario para D3DX)
    LPDIRECT3D9 m_pD3D;
    LPDIRECT3DDEVICE9 m_pDevice;

    /**
     * Inicializar dispositivo DirectX 9
     * @return true si se inicializó correctamente
     */
    bool InitializeD3D();

    /**
     * Liberar recursos de DirectX
     */
    void Shutdown();

    /**
     * Convertir jerarquía D3DXFRAME a FrameData
     * @param d3dFrame Frame de D3DX
     * @param parent Frame padre
     * @return FrameData convertido
     */
    FrameData* ConvertFrame(LPD3DXFRAME d3dFrame, FrameData* parent, vector<MaterialData>& materials);

    /**
     * Convertir D3DXMESHCONTAINER a MeshData
     * @param d3dMeshContainer Mesh container de D3DX
     * @return MeshData convertido
     */
    MeshData* ConvertMeshContainer(LPD3DXMESHCONTAINER d3dMeshContainer, vector<MaterialData>& materials);

    /**
     * Extraer skin weights de D3DXSKININFO
     * @param skinInfo Información de skinning
     * @param mesh Mesh a procesar
     */
    void ExtractSkinWeights(LPD3DXSKININFO skinInfo, MeshData* mesh);

    /**
     * Cargar animaciones desde ID3DXAnimationController
     * @param animController Controller de animación
     * @param sceneData [out] Escena donde guardar las animaciones
     */
    void LoadAnimations(ID3DXAnimationController* animController, SceneData& sceneData);

    /**
     * Calcular bounding box de la escena
     * @param sceneData Escena a procesar
     */
    void CalculateBoundingBox(SceneData& sceneData);

    /**
     * Clase auxiliar para allocación de jerarquías D3DX
     */
    class AllocateHierarchy : public ID3DXAllocateHierarchy
    {
    public:
        STDMETHOD(CreateFrame)(THIS_ LPCSTR Name, LPD3DXFRAME *ppNewFrame);
        STDMETHOD(CreateMeshContainer)(
            THIS_ LPCSTR Name,
            CONST D3DXMESHDATA *pMeshData,
            CONST D3DXMATERIAL *pMaterials,
            CONST D3DXEFFECTINSTANCE *pEffectInstances,
            DWORD NumMaterials,
            CONST DWORD *pAdjacency,
            LPD3DXSKININFO pSkinInfo,
            LPD3DXMESHCONTAINER *ppNewMeshContainer);
        STDMETHOD(DestroyFrame)(THIS_ LPD3DXFRAME pFrameToFree);
        STDMETHOD(DestroyMeshContainer)(THIS_ LPD3DXMESHCONTAINER pMeshContainerBase);
    };

    // Helper: Extraer vértices de un D3DX mesh
    bool ExtractVertices(LPD3DXMESH mesh, MeshData* meshData);

    // Helper: Extraer índices de un D3DX mesh
    bool ExtractIndices(LPD3DXMESH mesh, MeshData* meshData);

    // Helper: Extraer materiales
    void ExtractMaterials(
        CONST D3DXMATERIAL *pMaterials,
        DWORD NumMaterials,
        vector<MaterialData>& materials,
        MeshData* meshData);

    // Opciones de conversión actuales
    ConversionOptions m_Options;

    // Directorio del archivo actual (para texturas relativas)
    string m_CurrentDirectory;
};

#endif // XFILE_PARSER_H
