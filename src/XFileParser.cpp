#include "XFileParser.h"

// ============================================================================
// GUID Definitions para DirectX Animation Interfaces
// ============================================================================
// IID_ID3DXKeyframedAnimationSet no está exportado por d3dx9.lib en todas las versiones
// Por lo tanto, lo definimos manualmente aquí
// Este GUID permite hacer QueryInterface a ID3DXKeyframedAnimationSet
// ============================================================================

// Definición del GUID para ID3DXKeyframedAnimationSet
// Fuente: Microsoft DirectX SDK (June 2010) - d3dx9anim.h
static const GUID IID_ID3DXKeyframedAnimationSet =
{ 0xfa4e8e3a, 0x9786, 0x407d, { 0x8b, 0x4c, 0x59, 0x95, 0x89, 0x37, 0x64, 0xaf } };

// ============================================================================
// Constructor / Destructor
// ============================================================================

XFileParser::XFileParser()
    : m_pD3D(nullptr)
    , m_pDevice(nullptr)
{
}

XFileParser::~XFileParser()
{
    Shutdown();
}

// ============================================================================
// Inicialización de DirectX 9
// ============================================================================

bool XFileParser::InitializeD3D()
{
    // Crear objeto Direct3D
    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!m_pD3D)
    {
        Utils::LogError("Failed to create Direct3D9 object");
        return false;
    }

    // Parámetros de presentación (modo offscreen, no necesitamos renderizar)
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.BackBufferCount = 1;
    d3dpp.BackBufferWidth = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.hDeviceWindow = GetDesktopWindow();

    // Crear dispositivo (usando software VP para compatibilidad)
    HRESULT hr = m_pD3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_REF,  // Usar REF device (software)
        GetDesktopWindow(),
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp,
        &m_pDevice
    );

    if (FAILED(hr))
    {
        Utils::LogError("Failed to create Direct3D9 device");
        return false;
    }

    return true;
}

void XFileParser::Shutdown()
{
    if (m_pDevice)
    {
        m_pDevice->Release();
        m_pDevice = nullptr;
    }

    if (m_pD3D)
    {
        m_pD3D->Release();
        m_pD3D = nullptr;
    }
}

// ============================================================================
// Carga de Archivos .X
// ============================================================================

bool XFileParser::LoadFile(const string& filename, SceneData& sceneData, const ConversionOptions& options)
{
    m_Options = options;
    m_CurrentDirectory = Utils::GetDirectory(filename);

    Utils::Log("Loading .X file: " + filename, options.verbose);

    // Verificar que el archivo existe
    if (!Utils::FileExists(filename))
    {
        Utils::LogError("File not found: " + filename);
        return false;
    }

    // Inicializar DirectX si no está inicializado
    if (!m_pDevice)
    {
        if (!InitializeD3D())
            return false;
    }

    // Convertir filename a string ANSI (D3DXLoadMeshHierarchyFromX requiere ANSI)
    string ansiFilename = filename;

    // Usar allocator personalizado para la jerarquía
    AllocateHierarchy allocHierarchy;

    LPD3DXFRAME pFrameRoot = nullptr;
    ID3DXAnimationController* pAnimController = nullptr;

    // Cargar mesh hierarchy desde archivo .X
    HRESULT hr = D3DXLoadMeshHierarchyFromXA(
        ansiFilename.c_str(),
        D3DXMESH_MANAGED,
        m_pDevice,
        &allocHierarchy,
        nullptr,  // No user data callback
        &pFrameRoot,
        &pAnimController
    );

    if (FAILED(hr))
    {
        Utils::LogError("Failed to load .X file. HRESULT: 0x" +
                       std::to_string(hr));
        return false;
    }

    Utils::Log("Successfully loaded .X file hierarchy", options.verbose);

    // Convertir jerarquía D3DX a nuestra estructura
    sceneData.rootFrame = ConvertFrame(pFrameRoot, nullptr, sceneData.materials);

    // Cargar animaciones si existen
    if (pAnimController)
    {
        LoadAnimations(pAnimController, sceneData);
        pAnimController->Release();
    }

    // Limpiar jerarquía D3DX (ya la convertimos)
    D3DXFrameDestroy(pFrameRoot, &allocHierarchy);

    // Calcular bounding box
    CalculateBoundingBox(sceneData);

    Utils::Log("Conversion completed successfully", options.verbose);

    return true;
}

// ============================================================================
// Conversión de Jerarquía
// ============================================================================

FrameData* XFileParser::ConvertFrame(LPD3DXFRAME d3dFrame, FrameData* parent, vector<MaterialData>& materials)
{
    if (!d3dFrame)
        return nullptr;

    FrameData* frame = new FrameData();

    // Copiar nombre
    if (d3dFrame->Name)
        frame->name = string(d3dFrame->Name);

    // Copiar matriz de transformación
    frame->transformMatrix = d3dFrame->TransformationMatrix;
    frame->parent = parent;

    // Procesar mesh containers
    LPD3DXMESHCONTAINER pMeshContainer = d3dFrame->pMeshContainer;
    while (pMeshContainer)
    {
        MeshData* mesh = ConvertMeshContainer(pMeshContainer, materials);
        if (mesh)
        {
            frame->meshes.push_back(mesh);
        }
        pMeshContainer = pMeshContainer->pNextMeshContainer;
    }

    // Procesar hermanos (siblings)
    if (d3dFrame->pFrameSibling)
    {
        FrameData* sibling = ConvertFrame(d3dFrame->pFrameSibling, parent, materials);
        if (parent)
            parent->children.push_back(sibling);
    }

    // Procesar hijos
    if (d3dFrame->pFrameFirstChild)
    {
        FrameData* child = ConvertFrame(d3dFrame->pFrameFirstChild, frame, materials);
        frame->children.push_back(child);
    }

    return frame;
}

MeshData* XFileParser::ConvertMeshContainer(LPD3DXMESHCONTAINER d3dMeshContainer, vector<MaterialData>& materials)
{
    if (!d3dMeshContainer || !d3dMeshContainer->MeshData.pMesh)
        return nullptr;

    MeshData* mesh = new MeshData();

    // Nombre
    if (d3dMeshContainer->Name)
        mesh->name = string(d3dMeshContainer->Name);

    LPD3DXMESH pMesh = d3dMeshContainer->MeshData.pMesh;

    // Extraer vértices
    if (!ExtractVertices(pMesh, mesh))
    {
        delete mesh;
        return nullptr;
    }

    // Extraer índices
    if (!ExtractIndices(pMesh, mesh))
    {
        delete mesh;
        return nullptr;
    }

    // Extraer materiales
    if (d3dMeshContainer->pMaterials && d3dMeshContainer->NumMaterials > 0)
    {
        ExtractMaterials(
            d3dMeshContainer->pMaterials,
            d3dMeshContainer->NumMaterials,
            materials,
            mesh
        );
    }

    // Extraer skin weights si existe skinning
    if (d3dMeshContainer->pSkinInfo)
    {
        mesh->hasSkinning = true;
        ExtractSkinWeights(d3dMeshContainer->pSkinInfo, mesh);
    }

    return mesh;
}

// ============================================================================
// Extracción de Geometría
// ============================================================================

bool XFileParser::ExtractVertices(LPD3DXMESH mesh, MeshData* meshData)
{
    DWORD numVertices = mesh->GetNumVertices();
    DWORD fvf = mesh->GetFVF();

    BYTE* pVertices = nullptr;
    HRESULT hr = mesh->LockVertexBuffer(D3DLOCK_READONLY, (void**)&pVertices);
    if (FAILED(hr))
    {
        Utils::LogError("Failed to lock vertex buffer");
        return false;
    }

    // Determinar stride basado en FVF
    DWORD stride = D3DXGetFVFVertexSize(fvf);

    meshData->vertices.resize(numVertices);

    for (DWORD i = 0; i < numVertices; i++)
    {
        Vertex& vertex = meshData->vertices[i];
        BYTE* pVertex = pVertices + (i * stride);

        // Posición (siempre presente)
        if (fvf & D3DFVF_XYZ)
        {
            float* pPos = (float*)pVertex;
            vertex.position = D3DXVECTOR3(pPos[0], pPos[1], pPos[2]);
            pVertex += sizeof(float) * 3;
        }

        // Normal
        if (fvf & D3DFVF_NORMAL)
        {
            float* pNormal = (float*)pVertex;
            vertex.normal = D3DXVECTOR3(pNormal[0], pNormal[1], pNormal[2]);
            pVertex += sizeof(float) * 3;
        }

        // Coordenadas de textura
        if (fvf & D3DFVF_TEX1)
        {
            float* pTexCoord = (float*)pVertex;
            vertex.texCoord = D3DXVECTOR2(pTexCoord[0], pTexCoord[1]);
        }
    }

    mesh->UnlockVertexBuffer();
    return true;
}

bool XFileParser::ExtractIndices(LPD3DXMESH mesh, MeshData* meshData)
{
    DWORD numFaces = mesh->GetNumFaces();

    void* pIndices = nullptr;
    HRESULT hr = mesh->LockIndexBuffer(D3DLOCK_READONLY, &pIndices);
    if (FAILED(hr))
    {
        Utils::LogError("Failed to lock index buffer");
        return false;
    }

    // Determinar si son índices de 16 o 32 bits
    D3DINDEXBUFFER_DESC ibDesc;
    IDirect3DIndexBuffer9* pIB;
    mesh->GetIndexBuffer(&pIB);
    pIB->GetDesc(&ibDesc);
    pIB->Release();

    bool is32Bit = (ibDesc.Format == D3DFMT_INDEX32);

    meshData->indices.resize(numFaces * 3);

    if (is32Bit)
    {
        DWORD* pIndices32 = (DWORD*)pIndices;
        for (DWORD i = 0; i < numFaces * 3; i++)
        {
            meshData->indices[i] = pIndices32[i];
        }
    }
    else
    {
        WORD* pIndices16 = (WORD*)pIndices;
        for (DWORD i = 0; i < numFaces * 3; i++)
        {
            meshData->indices[i] = (DWORD)pIndices16[i];
        }
    }

    mesh->UnlockIndexBuffer();
    return true;
}

// ============================================================================
// Extracción de Skin Weights
// ============================================================================
// EXTRACCIÓN DE SKIN WEIGHTS (Pesos de Influencia de Huesos)
// ============================================================================
// Esta función extrae la información de skinning del archivo .X.
//
// ¿Qué es skinning?
//   Es la técnica que permite que un mesh (piel) se deforme siguiendo
//   el movimiento de un skeleton (esqueleto de huesos).
//
// Cada vértice puede estar influenciado por múltiples huesos.
// Ejemplo: El codo de un brazo está influenciado por:
//   - 70% por el hueso del antebrazo
//   - 30% por el hueso del brazo superior
//
// Datos que se extraen:
//   - Nombre de cada hueso
//   - Matriz de offset (bind pose matrix / inverse bind pose)
//   - Qué vértices influencia cada hueso
//   - Cuánto influencia cada hueso a cada vértice (peso 0.0 - 1.0)
// ============================================================================

void XFileParser::ExtractSkinWeights(LPD3DXSKININFO skinInfo, MeshData* mesh)
{
    // Obtener número total de huesos en el skeleton
    DWORD numBones = skinInfo->GetNumBones();

    mesh->bones.resize(numBones);

    // Iterar sobre cada hueso del skeleton para extraer sus influencias
    for (DWORD iBone = 0; iBone < numBones; iBone++)
    {
        BoneData& bone = mesh->bones[iBone];

        // ====================================================================
        // Extraer nombre del hueso (ej: "Spine", "LeftArm", "Head")
        // ====================================================================
        bone.name = string(skinInfo->GetBoneName(iBone));

        // ====================================================================
        // Extraer matriz de offset (inverse bind pose matrix)
        // ====================================================================
        // Esta matriz transforma vértices del espacio del mesh al espacio del hueso.
        // Durante la animación:
        //   VertexFinal = (OffsetMatrix * BoneMatrix_Current) * VertexOriginal
        bone.offsetMatrix = *(skinInfo->GetBoneOffsetMatrix(iBone));

        // ====================================================================
        // Obtener número de vértices influenciados por este hueso
        // ====================================================================
        DWORD numInfluences = skinInfo->GetNumBoneInfluences(iBone);

        // Alocar arrays temporales para índices de vértices y sus pesos
        DWORD* pVertices = new DWORD[numInfluences];  // Índices de vértices
        float* pWeights = new float[numInfluences];   // Pesos (0.0 - 1.0)

        // ====================================================================
        // Obtener influencias del hueso usando la API de DirectX
        // ====================================================================
        // GetBoneInfluence() llena los arrays con:
        //   - pVertices[i]: índice del vértice i-ésimo influenciado
        //   - pWeights[i]: cuánto influye este hueso (0.0=nada, 1.0=totalmente)
        DWORD actualInfluences = skinInfo->GetBoneInfluence(iBone, pVertices, pWeights);
        if (actualInfluences == 0 || actualInfluences > numInfluences)
        {
            delete[] pVertices;
            delete[] pWeights;
            continue;
        }

        // ====================================================================
        // Asignar pesos a cada vértice influenciado
        // ====================================================================
        for (DWORD iInfl = 0; iInfl < actualInfluences; iInfl++)
        {
            DWORD vertexIndex = pVertices[iInfl];  // Índice del vértice
            float weight = pWeights[iInfl];        // Peso de influencia

            // Validar que el índice de vértice sea válido
            if (vertexIndex < mesh->vertices.size())
            {
                Vertex& vertex = mesh->vertices[vertexIndex];

                // ============================================================
                // Encontrar slot libre para almacenar este peso
                // ============================================================
                // Cada vértice puede tener hasta MAX_BONE_INFLUENCES (4) huesos.
                // Esto es estándar en la industria (GPU limitado a 4 influencias).
                for (int iSlot = 0; iSlot < MAX_BONE_INFLUENCES; iSlot++)
                {
                    if (vertex.boneWeights[iSlot] == 0.0f)
                    {
                        vertex.boneIndices[iSlot] = iBone;  // Guardar índice del hueso
                        vertex.boneWeights[iSlot] = weight; // Guardar peso
                        break;  // Slot encontrado, salir del bucle
                    }
                }
            }
        }

        // Liberar memoria temporal
        delete[] pVertices;
        delete[] pWeights;
    }

    // ========================================================================
    // NORMALIZACIÓN DE PESOS (CRÍTICO para animación correcta)
    // ========================================================================
    // Los pesos de cada vértice DEBEN sumar exactamente 1.0
    // De lo contrario, el mesh se deformará incorrectamente.
    //
    // Ejemplo:
    //   Antes:  [0.7, 0.2, 0.05, 0.0] = suma 0.95 ❌
    //   Después: [0.737, 0.211, 0.053, 0.0] = suma 1.0 ✅
    // ========================================================================
    for (Vertex& vertex : mesh->vertices)
    {
        // Calcular suma total de pesos de este vértice
        float totalWeight = 0.0f;
        for (int i = 0; i < MAX_BONE_INFLUENCES; i++)
            totalWeight += vertex.boneWeights[i];

        // Si hay pesos válidos (suma > 0), normalizar dividiéndolos por el total
        if (totalWeight > EPSILON)  // EPSILON evita división por cero
        {
            for (int i = 0; i < MAX_BONE_INFLUENCES; i++)
                vertex.boneWeights[i] /= totalWeight;  // Dividir cada peso por el total
        }
    }
}

// ============================================================================
// Carga de Animaciones
// ============================================================================

// ============================================================================
// CARGA DE ANIMACIONES
// ============================================================================
// Esta función extrae los clips de animación del ID3DXAnimationController
// que fue cargado desde el archivo .X junto con la jerarquía de frames.
//
// PROBLEMA ACTUAL: Solo extrae metadatos (nombre, duración) pero NO los keyframes.
// Los keyframes son necesarios para que las animaciones funcionen en FBX.
// ============================================================================
void XFileParser::LoadAnimations(ID3DXAnimationController* animController, SceneData& sceneData)
{
    // Obtener número de animation sets (clips) en el controlador
    UINT numAnimSets = animController->GetNumAnimationSets();

    // ========================================================================
    // LOGGING DE PROGRESO PARA GRANDES CANTIDADES DE ANIMACIONES
    // ========================================================================
    if (numAnimSets > 10)
    {
        cout << "Loading " << numAnimSets << " animation(s)... This may take a while.\n";
    }

    // Iterar sobre cada animation set
    for (UINT iSet = 0; iSet < numAnimSets; iSet++)
    {
        // Mostrar progreso cada 10 animaciones
        if (numAnimSets > 10 && iSet % 10 == 0)
        {
            cout << "Progress: " << iSet << "/" << numAnimSets << " animations loaded...\n";
        }

        LPD3DXANIMATIONSET pAnimSet;
        animController->GetAnimationSet(iSet, &pAnimSet);

        // Crear clip de animación
        AnimationClip clip;

        // Extraer metadatos básicos
        clip.name = string(pAnimSet->GetName());           // Nombre del clip (ej: "Walk", "Run")
        clip.duration = pAnimSet->GetPeriod();             // Duración en segundos
        clip.ticksPerSecond = pAnimSet->GetPeriodicPosition(1.0) / pAnimSet->GetPeriod();  // FPS

        // ========================================================================
        // EXTRACCIÓN DE KEYFRAMES (IMPLEMENTACIÓN COMPLETA)
        // ========================================================================
        // Se hace QueryInterface a ID3DXKeyframedAnimationSet para obtener
        // acceso a los keyframes individuales de cada track de animación.
        // ========================================================================

        ID3DXKeyframedAnimationSet* pKeyframedSet = nullptr;
        if (SUCCEEDED(pAnimSet->QueryInterface(IID_ID3DXKeyframedAnimationSet, (void**)&pKeyframedSet)))
        {
            UINT numAnimations = pKeyframedSet->GetNumAnimations();

            // Iterar sobre cada animation (track por hueso)
            for (UINT iAnim = 0; iAnim < numAnimations; iAnim++)
            {
                AnimationTrack track;

                // Obtener nombre del hueso que esta animación controla
                const char* boneName = nullptr;
                pKeyframedSet->GetAnimationNameByIndex(iAnim, &boneName);

                // ============================================================
                // VALIDACIÓN: Verificar que boneName no sea nullptr
                // ============================================================
                if (boneName == nullptr || strlen(boneName) == 0)
                {
                    continue;  // Saltar este track si no tiene nombre válido
                }

                track.boneName = string(boneName);

                // ================================================================
                // EXTRACCIÓN DE KEYFRAMES DE ROTACIÓN
                // ================================================================
                UINT numRotKeys = pKeyframedSet->GetNumRotationKeys(iAnim);
                if (numRotKeys > 0)
                {
                    D3DXKEY_QUATERNION* pRotKeys = new D3DXKEY_QUATERNION[numRotKeys];
                    pKeyframedSet->GetRotationKeys(iAnim, pRotKeys);

                    // Pre-reservar memoria para eficiencia
                    track.keys.reserve(numRotKeys);

                    // Crear keyframe para cada rotación
                    for (UINT iKey = 0; iKey < numRotKeys; iKey++)
                    {
                        AnimationKey key;
                        key.time = pRotKeys[iKey].Time;
                        key.rotation = pRotKeys[iKey].Value;

                        // Inicializar translation y scale por defecto
                        key.translation = D3DXVECTOR3(0, 0, 0);
                        key.scale = D3DXVECTOR3(1, 1, 1);

                        track.keys.push_back(key);
                    }
                    delete[] pRotKeys;
                }

                // ================================================================
                // EXTRACCIÓN DE KEYFRAMES DE TRASLACIÓN
                // ================================================================
                // OPTIMIZADO: Usar map temporal para fusión O(log n) en vez de O(n²)
                // ================================================================
                UINT numPosKeys = pKeyframedSet->GetNumTranslationKeys(iAnim);
                if (numPosKeys > 0)
                {
                    D3DXKEY_VECTOR3* pPosKeys = new D3DXKEY_VECTOR3[numPosKeys];
                    pKeyframedSet->GetTranslationKeys(iAnim, pPosKeys);

                    // Si no hay keyframes de rotación, crear nuevos keyframes directamente
                    if (track.keys.empty())
                    {
                        track.keys.reserve(numPosKeys);  // Pre-reservar memoria
                        for (UINT iKey = 0; iKey < numPosKeys; iKey++)
                        {
                            AnimationKey key;
                            key.time = pPosKeys[iKey].Time;
                            key.translation = pPosKeys[iKey].Value;
                            key.rotation = D3DXQUATERNION(0, 0, 0, 1);
                            key.scale = D3DXVECTOR3(1, 1, 1);
                            track.keys.push_back(key);
                        }
                    }
                    else
                    {
                        // Crear mapa temporal para búsqueda rápida O(log n)
                        map<double, size_t> timeToIndex;
                        for (size_t i = 0; i < track.keys.size(); i++)
                        {
                            timeToIndex[track.keys[i].time] = i;
                        }

                        // Fusionar con keyframes existentes usando el mapa
                        for (UINT iKey = 0; iKey < numPosKeys; iKey++)
                        {
                            double time = pPosKeys[iKey].Time;
                            auto it = timeToIndex.find(time);

                            if (it != timeToIndex.end())
                            {
                                // Actualizar keyframe existente
                                track.keys[it->second].translation = pPosKeys[iKey].Value;
                            }
                            else
                            {
                                // Crear nuevo keyframe
                                AnimationKey key;
                                key.time = time;
                                key.translation = pPosKeys[iKey].Value;
                                key.rotation = D3DXQUATERNION(0, 0, 0, 1);
                                key.scale = D3DXVECTOR3(1, 1, 1);
                                track.keys.push_back(key);
                            }
                        }
                    }
                    delete[] pPosKeys;
                }

                // ================================================================
                // EXTRACCIÓN DE KEYFRAMES DE ESCALA
                // ================================================================
                // OPTIMIZADO: Usar map temporal para fusión O(log n) en vez de O(n²)
                // ================================================================
                UINT numScaleKeys = pKeyframedSet->GetNumScaleKeys(iAnim);
                if (numScaleKeys > 0)
                {
                    D3DXKEY_VECTOR3* pScaleKeys = new D3DXKEY_VECTOR3[numScaleKeys];
                    pKeyframedSet->GetScaleKeys(iAnim, pScaleKeys);

                    // Si no hay keyframes, crear nuevos directamente
                    if (track.keys.empty())
                    {
                        track.keys.reserve(numScaleKeys);  // Pre-reservar memoria
                        for (UINT iKey = 0; iKey < numScaleKeys; iKey++)
                        {
                            AnimationKey key;
                            key.time = pScaleKeys[iKey].Time;
                            key.scale = pScaleKeys[iKey].Value;
                            key.translation = D3DXVECTOR3(0, 0, 0);
                            key.rotation = D3DXQUATERNION(0, 0, 0, 1);
                            track.keys.push_back(key);
                        }
                    }
                    else
                    {
                        // Crear mapa temporal para búsqueda rápida O(log n)
                        map<double, size_t> timeToIndex;
                        for (size_t i = 0; i < track.keys.size(); i++)
                        {
                            timeToIndex[track.keys[i].time] = i;
                        }

                        // Fusionar con keyframes existentes usando el mapa
                        for (UINT iKey = 0; iKey < numScaleKeys; iKey++)
                        {
                            double time = pScaleKeys[iKey].Time;
                            auto it = timeToIndex.find(time);

                            if (it != timeToIndex.end())
                            {
                                // Actualizar keyframe existente
                                track.keys[it->second].scale = pScaleKeys[iKey].Value;
                            }
                            else
                            {
                                // Crear nuevo keyframe
                                AnimationKey key;
                                key.time = time;
                                key.scale = pScaleKeys[iKey].Value;
                                key.translation = D3DXVECTOR3(0, 0, 0);
                                key.rotation = D3DXQUATERNION(0, 0, 0, 1);
                                track.keys.push_back(key);
                            }
                        }
                    }
                    delete[] pScaleKeys;
                }

                // Solo agregar el track si tiene keyframes
                if (!track.keys.empty())
                {
                    // ============================================================
                    // WARNING: Detectar cantidades anormales de keyframes
                    // ============================================================
                    if (track.keys.size() > 10000)
                    {
                        cout << "  WARNING: Track '" << track.boneName
                             << "' has " << track.keys.size()
                             << " keyframes (unusually high)\n";
                    }

                    clip.tracks.push_back(track);
                }
            }

            pKeyframedSet->Release();
        }
        // ========================================================================

        // Agregar clip al scene data (aunque esté incompleto)
        sceneData.animations.push_back(clip);

        pAnimSet->Release();
    }

    // Logging final de progreso
    if (numAnimSets > 10)
    {
        cout << "Progress: " << numAnimSets << "/" << numAnimSets << " animations loaded successfully!\n";
    }
}

// ============================================================================
// Helpers
// ============================================================================

void XFileParser::CalculateBoundingBox(SceneData& sceneData)
{
    // Recorrer todos los frames y meshes para calcular bounding box
    // (implementación recursiva)
    // Por simplicidad, inicializamos con valores por defecto
    sceneData.boundingBoxMin = D3DXVECTOR3(-100, -100, -100);
    sceneData.boundingBoxMax = D3DXVECTOR3(100, 100, 100);
}

bool XFileParser::GetFileInfo(const string& filename, int& numMeshes, int& numBones, int& numAnimations)
{
    // Implementación simplificada
    numMeshes = 0;
    numBones = 0;
    numAnimations = 0;

    // Cargar archivo y contar elementos
    SceneData sceneData;
    ConversionOptions tempOptions;
    tempOptions.verbose = false;

    if (!LoadFile(filename, sceneData, tempOptions))
        return false;

    // Contar elementos (implementar recorrido recursivo)

    return true;
}

// ============================================================================
// AllocateHierarchy Implementation (igual que el código de ejemplo)
// ============================================================================

HRESULT XFileParser::AllocateHierarchy::CreateFrame(LPCSTR Name, LPD3DXFRAME *ppNewFrame)
{
    D3DXFRAME* pFrame = new D3DXFRAME;
    ZeroMemory(pFrame, sizeof(D3DXFRAME));

    if (Name)
    {
        size_t len = strlen(Name) + 1;
        pFrame->Name = new char[len];
        strcpy_s(pFrame->Name, len, Name);
    }

    D3DXMatrixIdentity(&pFrame->TransformationMatrix);

    *ppNewFrame = pFrame;
    return S_OK;
}

HRESULT XFileParser::AllocateHierarchy::CreateMeshContainer(
    LPCSTR Name,
    CONST D3DXMESHDATA *pMeshData,
    CONST D3DXMATERIAL *pMaterials,
    CONST D3DXEFFECTINSTANCE *pEffectInstances,
    DWORD NumMaterials,
    CONST DWORD *pAdjacency,
    LPD3DXSKININFO pSkinInfo,
    LPD3DXMESHCONTAINER *ppNewMeshContainer)
{
    D3DXMESHCONTAINER* pMeshContainer = new D3DXMESHCONTAINER;
    ZeroMemory(pMeshContainer, sizeof(D3DXMESHCONTAINER));

    if (Name)
    {
        size_t len = strlen(Name) + 1;
        pMeshContainer->Name = new char[len];
        strcpy_s(pMeshContainer->Name, len, Name);
    }

    // Copiar mesh data
    pMeshContainer->MeshData = *pMeshData;
    if (pMeshData->pMesh)
        pMeshData->pMesh->AddRef();

    // Copiar materiales
    pMeshContainer->NumMaterials = NumMaterials;
    if (NumMaterials > 0)
    {
        pMeshContainer->pMaterials = new D3DXMATERIAL[NumMaterials];
        memcpy(pMeshContainer->pMaterials, pMaterials, sizeof(D3DXMATERIAL) * NumMaterials);
    }

    // Copiar skin info
    if (pSkinInfo)
    {
        pMeshContainer->pSkinInfo = pSkinInfo;
        pSkinInfo->AddRef();
    }

    *ppNewMeshContainer = pMeshContainer;
    return S_OK;
}

HRESULT XFileParser::AllocateHierarchy::DestroyFrame(LPD3DXFRAME pFrameToFree)
{
    if (pFrameToFree->Name)
        delete[] pFrameToFree->Name;
    delete pFrameToFree;
    return S_OK;
}

HRESULT XFileParser::AllocateHierarchy::DestroyMeshContainer(LPD3DXMESHCONTAINER pMeshContainerBase)
{
    if (pMeshContainerBase->Name)
        delete[] pMeshContainerBase->Name;
    if (pMeshContainerBase->pMaterials)
        delete[] pMeshContainerBase->pMaterials;
    if (pMeshContainerBase->pSkinInfo)
        pMeshContainerBase->pSkinInfo->Release();
    if (pMeshContainerBase->MeshData.pMesh)
        pMeshContainerBase->MeshData.pMesh->Release();

    delete pMeshContainerBase;
    return S_OK;
}

void XFileParser::ExtractMaterials(
    CONST D3DXMATERIAL *pMaterials,
    DWORD NumMaterials,
    vector<MaterialData>& materials,
    MeshData* meshData)
{
    for (DWORD i = 0; i < NumMaterials; i++)
    {
        MaterialData matData;
        matData.material = pMaterials[i].MatD3D;

        if (pMaterials[i].pTextureFilename)
        {
            matData.textureFilename = string(pMaterials[i].pTextureFilename);

            // Convertir a ruta absoluta si es relativa
            if (!m_CurrentDirectory.empty())
            {
                string fullPath = m_CurrentDirectory + matData.textureFilename;
                if (Utils::FileExists(fullPath))
                    matData.textureFilename = fullPath;
            }
        }

        matData.name = "Material_" + to_string(materials.size());

        materials.push_back(matData);
        meshData->materialIndices.push_back(i);
    }
}
