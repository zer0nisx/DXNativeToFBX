#include "FBXExporter.h"
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================================
// Constructor / Destructor
// ============================================================================

FBXExporter::FBXExporter()
    : m_pManager(nullptr)
    , m_pScene(nullptr)
{
}

FBXExporter::~FBXExporter()
{
    Shutdown();
}

// ============================================================================
// Inicialización
// ============================================================================

bool FBXExporter::Initialize()
{
    // Si ya está inicializado, solo crear una nueva escena
    if (m_pManager)
    {
        // Destruir escena anterior si existe
        if (m_pScene)
        {
            m_pScene->Destroy();
            m_pScene = nullptr;
        }

        // Crear nueva escena
        m_pScene = FbxScene::Create(m_pManager, "Scene");
        if (!m_pScene)
        {
            m_LastError = "Failed to create FBX Scene";
            return false;
        }
        return true;
    }

    // Crear FBX Manager (primera vez)
    m_pManager = FbxManager::Create();
    if (!m_pManager)
    {
        m_LastError = "Failed to create FBX Manager";
        return false;
    }

    // Crear IO Settings
    FbxIOSettings* ios = FbxIOSettings::Create(m_pManager, IOSROOT);
    m_pManager->SetIOSettings(ios);

    // Crear Scene
    m_pScene = FbxScene::Create(m_pManager, "Scene");
    if (!m_pScene)
    {
        m_LastError = "Failed to create FBX Scene";
        return false;
    }

    return true;
}

void FBXExporter::Shutdown()
{
    if (m_pScene)
    {
        m_pScene->Destroy();
        m_pScene = nullptr;
    }

    if (m_pManager)
    {
        m_pManager->Destroy();
        m_pManager = nullptr;
    }

    m_BoneNodeMap.clear();
}

// ============================================================================
// Exportación Principal
// ============================================================================

bool FBXExporter::ExportScene(
    const SceneData& sceneData,
    const string& filename,
    const ConversionOptions& options)
{
    m_Options = options;

    Utils::Log("Starting FBX export to: " + filename, options.verbose);

    // Inicializar FBX SDK
    if (!Initialize())
    {
        Utils::LogError(m_LastError);
        return false;
    }

    // Crear escena FBX desde los datos
    if (!CreateFBXScene(sceneData))
    {
        Utils::LogError(m_LastError);
        return false;
    }

    // Configurar propiedades de la escena
    SetupSceneProperties();
    SetupCoordinateSystem();

    // Crear exporter
    FbxExporter* pExporter = FbxExporter::Create(m_pScene, "");

    // Determinar formato de exportación
    int format = m_pManager->GetIOPluginRegistry()->GetNativeWriterFormat();

    // FBX SDK 2020.3.7: Use default format or find by description
    // The eFBX_* constants no longer exist, so we use the registry
    if (format < 0)
    {
        format = m_pManager->GetIOPluginRegistry()->FindWriterIDByDescription("FBX binary (*.fbx)");
    }

    // Inicializar exporter
    if (!pExporter->Initialize(filename.c_str(), format, m_pManager->GetIOSettings()))
    {
        m_LastError = "Failed to initialize FBX Exporter: " + string(pExporter->GetStatus().GetErrorString());
        pExporter->Destroy();
        return false;
    }

    // Exportar escena
    bool result = pExporter->Export(m_pScene);

    if (!result)
    {
        m_LastError = "Failed to export FBX: " + string(pExporter->GetStatus().GetErrorString());
    }
    else
    {
        Utils::Log("FBX export completed successfully", options.verbose);
    }

    pExporter->Destroy();

    return result;
}

// ============================================================================
// Exportar Animación Individual
// ============================================================================

bool FBXExporter::ExportSingleAnimation(
    const SceneData& sceneData,
    const AnimationClip& animation,
    const string& filename,
    const ConversionOptions& options)
{
    m_Options = options;

    Utils::Log("Exporting animation '" + animation.name + "' to: " + filename, options.verbose);

    // Limpiar mapa de huesos para esta animación
    m_BoneNodeMap.clear();

    // Inicializar FBX SDK (creará nueva escena si es necesario)
    if (!Initialize())
    {
        Utils::LogError(m_LastError);
        return false;
    }

    // Crear escena FBX desde los datos (sin animaciones)
    if (!sceneData.rootFrame)
    {
        m_LastError = "No root frame in scene data";
        return false;
    }

    // Obtener nodo raíz de la escena
    FbxNode* rootNode = m_pScene->GetRootNode();

    // Exportar jerarquía de frames (sin animaciones)
    ExportFrame(sceneData.rootFrame, rootNode);

    // Exportar solo esta animación
    ExportAnimationClip(animation);

    // Configurar propiedades de la escena
    SetupSceneProperties();
    SetupCoordinateSystem();

    // Crear exporter
    FbxExporter* pExporter = FbxExporter::Create(m_pScene, "");

    // Determinar formato de exportación
    int format = m_pManager->GetIOPluginRegistry()->GetNativeWriterFormat();
    if (format < 0)
    {
        format = m_pManager->GetIOPluginRegistry()->FindWriterIDByDescription("FBX binary (*.fbx)");
    }

    // Inicializar exporter
    if (!pExporter->Initialize(filename.c_str(), format, m_pManager->GetIOSettings()))
    {
        m_LastError = "Failed to initialize FBX Exporter: " + string(pExporter->GetStatus().GetErrorString());
        pExporter->Destroy();
        return false;
    }

    // Exportar escena
    bool result = pExporter->Export(m_pScene);

    if (!result)
    {
        m_LastError = "Failed to export FBX: " + string(pExporter->GetStatus().GetErrorString());
    }
    else
    {
        Utils::Log("Animation export completed successfully", options.verbose);
    }

    pExporter->Destroy();

    return result;
}

// ============================================================================
// Creación de Escena FBX
// ============================================================================

bool FBXExporter::CreateFBXScene(const SceneData& sceneData)
{
    if (!sceneData.rootFrame)
    {
        m_LastError = "No root frame in scene data";
        return false;
    }

    // Obtener nodo raíz de la escena
    FbxNode* rootNode = m_pScene->GetRootNode();

    // Exportar jerarquía de frames
    ExportFrame(sceneData.rootFrame, rootNode);

    // Exportar animaciones
    if (!sceneData.animations.empty())
    {
        ExportAnimations(sceneData.animations);
    }

    return true;
}

// ============================================================================
// Exportación de Frames (Jerarquía)
// ============================================================================

FbxNode* FBXExporter::ExportFrame(FrameData* frameData, FbxNode* parentNode)
{
    if (!frameData)
        return nullptr;

    // Crear nodo FBX para este frame
    FbxNode* node = FbxNode::Create(m_pScene, frameData->name.c_str());

    // Convertir matriz de transformación
    FbxAMatrix transform = MatrixConverter::ConvertMatrixWithOptions(
        frameData->transformMatrix,
        m_Options
    );

    // Aplicar transformación al nodo
    node->LclTranslation.Set(transform.GetT());
    node->LclRotation.Set(transform.GetR());
    node->LclScaling.Set(transform.GetS());

    // Agregar al padre
    parentNode->AddChild(node);

    // Registrar en mapa de huesos (para animaciones)
    if (!frameData->name.empty())
    {
        m_BoneNodeMap[frameData->name] = node;
    }

    // Exportar meshes de este frame
    for (MeshData* mesh : frameData->meshes)
    {
        // Necesitamos pasar materials, pero no están en frameData
        // Los obtendremos del sceneData global
        // Por ahora, usamos vector vacío
        vector<MaterialData> emptyMaterials;
        ExportMesh(mesh, node, emptyMaterials);
    }

    // Exportar hijos recursivamente
    for (FrameData* child : frameData->children)
    {
        ExportFrame(child, node);
    }

    return node;
}

// ============================================================================
// Exportación de Mesh
// ============================================================================

FbxNode* FBXExporter::ExportMesh(
    MeshData* meshData,
    FbxNode* frameNode,
    const vector<MaterialData>& materials)
{
    if (!meshData || meshData->vertices.empty())
        return nullptr;

    // Crear FbxMesh
    FbxMesh* fbxMesh = FbxMesh::Create(m_pScene, meshData->name.c_str());

    // Exportar geometría
    ExportGeometry(meshData, fbxMesh);

    // Exportar UVs
    ExportUVs(meshData, fbxMesh);

    // Exportar normales
    ExportNormals(meshData, fbxMesh);

    // Crear nodo para el mesh
    FbxNode* meshNode = FbxNode::Create(m_pScene, (meshData->name + "_node").c_str());
    meshNode->SetNodeAttribute(fbxMesh);

    // Exportar materiales
    if (!materials.empty())
    {
        ExportMaterials(materials, meshNode, meshData);
    }

    // Exportar skinning si existe
    if (meshData->hasSkinning && !meshData->bones.empty())
    {
        // Necesitamos el rootFrame para encontrar los huesos
        // Por ahora, lo dejamos comentado
        // ExportSkeleton(meshData, meshNode, rootFrame);
        ExportSkinWeights(meshData, fbxMesh, meshNode);
    }

    // Agregar mesh node como hijo del frame
    frameNode->AddChild(meshNode);

    return meshNode;
}

// ============================================================================
// Exportación de Geometría
// ============================================================================

void FBXExporter::ExportGeometry(MeshData* meshData, FbxMesh* fbxMesh)
{
    int numVertices = (int)meshData->vertices.size();
    int numPolygons = (int)meshData->indices.size() / 3;

    // Inicializar control points (vértices)
    fbxMesh->InitControlPoints(numVertices);
    FbxVector4* controlPoints = fbxMesh->GetControlPoints();

    // Copiar posiciones de vértices
    for (int i = 0; i < numVertices; i++)
    {
        const Vertex& vertex = meshData->vertices[i];
        FbxVector4 pos = MatrixConverter::ConvertPosition_LH_to_RH(vertex.position);

        // Aplicar escala global
        if (m_Options.scale != 1.0f)
        {
            pos = MatrixConverter::ApplyGlobalScale(pos, m_Options.scale);
        }

        controlPoints[i] = pos;
    }

    // Crear polígonos (triángulos)
    for (int i = 0; i < numPolygons; i++)
    {
        fbxMesh->BeginPolygon(-1, -1, false);

        // DirectX usa winding order clockwise, FBX usa counter-clockwise
        // Invertir orden de vértices si estamos convirtiendo a RH
        if (m_Options.targetCoordSystem == CoordinateSystem::RIGHT_HANDED)
        {
            fbxMesh->AddPolygon(meshData->indices[i * 3 + 0]);
            fbxMesh->AddPolygon(meshData->indices[i * 3 + 2]);  // Invertir
            fbxMesh->AddPolygon(meshData->indices[i * 3 + 1]);  // Invertir
        }
        else
        {
            fbxMesh->AddPolygon(meshData->indices[i * 3 + 0]);
            fbxMesh->AddPolygon(meshData->indices[i * 3 + 1]);
            fbxMesh->AddPolygon(meshData->indices[i * 3 + 2]);
        }

        fbxMesh->EndPolygon();
    }
}

void FBXExporter::ExportUVs(MeshData* meshData, FbxMesh* fbxMesh)
{
    // Crear layer de UVs
    FbxGeometryElementUV* uvElement = fbxMesh->CreateElementUV("DiffuseUV");
    uvElement->SetMappingMode(FbxGeometryElement::eByControlPoint);
    uvElement->SetReferenceMode(FbxGeometryElement::eDirect);

    // Agregar UVs
    for (const Vertex& vertex : meshData->vertices)
    {
        FbxVector2 uv(vertex.texCoord.x, 1.0 - vertex.texCoord.y);  // Invertir V (DirectX vs FBX)
        uvElement->GetDirectArray().Add(uv);
    }
}

void FBXExporter::ExportNormals(MeshData* meshData, FbxMesh* fbxMesh)
{
    // Crear layer de normales
    FbxGeometryElementNormal* normalElement = fbxMesh->CreateElementNormal();
    normalElement->SetMappingMode(FbxGeometryElement::eByControlPoint);
    normalElement->SetReferenceMode(FbxGeometryElement::eDirect);

    // Agregar normales
    for (const Vertex& vertex : meshData->vertices)
    {
        FbxVector4 normal = MatrixConverter::ConvertNormal_LH_to_RH(vertex.normal);
        normalElement->GetDirectArray().Add(normal);
    }
}

// ============================================================================
// Exportación de Materiales
// ============================================================================

void FBXExporter::ExportMaterials(
    const vector<MaterialData>& materials,
    FbxNode* meshNode,
    MeshData* meshData)
{
    for (size_t i = 0; i < materials.size(); i++)
    {
        FbxSurfacePhong* fbxMaterial = CreateMaterial(materials[i]);
        meshNode->AddMaterial(fbxMaterial);
    }

    // Si hay materiales por triángulo, crear element de material
    if (!meshData->materialIndices.empty())
    {
        FbxMesh* fbxMesh = meshNode->GetMesh();
        FbxGeometryElementMaterial* matElement = fbxMesh->CreateElementMaterial();
        matElement->SetMappingMode(FbxGeometryElement::eByPolygon);
        matElement->SetReferenceMode(FbxGeometryElement::eIndexToDirect);

        // Asignar índice de material a cada polígono
        for (DWORD matIndex : meshData->materialIndices)
        {
            matElement->GetIndexArray().Add(matIndex);
        }
    }
}

FbxSurfacePhong* FBXExporter::CreateMaterial(const MaterialData& matData)
{
    FbxSurfacePhong* material = FbxSurfacePhong::Create(m_pScene, matData.name.c_str());

    // Configurar propiedades del material
    material->Diffuse.Set(FbxDouble3(
        matData.material.Diffuse.r,
        matData.material.Diffuse.g,
        matData.material.Diffuse.b
    ));

    material->Ambient.Set(FbxDouble3(
        matData.material.Ambient.r,
        matData.material.Ambient.g,
        matData.material.Ambient.b
    ));

    material->Specular.Set(FbxDouble3(
        matData.material.Specular.r,
        matData.material.Specular.g,
        matData.material.Specular.b
    ));

    material->Shininess.Set(matData.material.Power);
    material->Emissive.Set(FbxDouble3(
        matData.material.Emissive.r,
        matData.material.Emissive.g,
        matData.material.Emissive.b
    ));

    // Agregar textura si existe
    if (!matData.textureFilename.empty())
    {
        FbxFileTexture* texture = FbxFileTexture::Create(m_pScene, "DiffuseTexture");

        string texturePath = matData.textureFilename;

        // Copiar textura si se solicitó
        if (m_Options.exportTextures)
        {
            texturePath = CopyTexture(matData.textureFilename);
        }

        texture->SetFileName(texturePath.c_str());
        texture->SetTextureUse(FbxTexture::eStandard);
        texture->SetMappingType(FbxTexture::eUV);
        texture->SetMaterialUse(FbxFileTexture::eModelMaterial);
        texture->SetSwapUV(false);
        texture->SetTranslation(0.0, 0.0);
        texture->SetScale(1.0, 1.0);
        texture->SetRotation(0.0, 0.0);

        material->Diffuse.ConnectSrcObject(texture);
    }

    return material;
}

// ============================================================================
// Exportación de Skinning
// ============================================================================

void FBXExporter::ExportSkinWeights(
    MeshData* meshData,
    FbxMesh* fbxMesh,
    FbxNode* meshNode)
{
    if (!meshData->hasSkinning || meshData->bones.empty())
        return;

    // Crear FbxSkin
    FbxSkin* skin = FbxSkin::Create(m_pScene, "");

    // Crear cluster para cada hueso
    for (size_t iBone = 0; iBone < meshData->bones.size(); iBone++)
    {
        const BoneData& bone = meshData->bones[iBone];

        // Buscar nodo del hueso
        FbxNode* boneNode = nullptr;
        auto it = m_BoneNodeMap.find(bone.name);
        if (it != m_BoneNodeMap.end())
        {
            boneNode = it->second;
        }
        else
        {
            // Si no existe, crear un hueso dummy
            boneNode = CreateBone(bone.name, bone.transformMatrix, m_pScene->GetRootNode());
        }

        // Crear cluster
        FbxCluster* cluster = FbxCluster::Create(m_pScene, "");
        cluster->SetLink(boneNode);
        cluster->SetLinkMode(FbxCluster::eTotalOne);

        // Agregar vértices influenciados por este hueso
        for (size_t iVert = 0; iVert < meshData->vertices.size(); iVert++)
        {
            const Vertex& vertex = meshData->vertices[iVert];

            for (int iInfl = 0; iInfl < MAX_BONE_INFLUENCES; iInfl++)
            {
                if (vertex.boneIndices[iInfl] == iBone && vertex.boneWeights[iInfl] > 0.0f)
                {
                    cluster->AddControlPointIndex((int)iVert, vertex.boneWeights[iInfl]);
                }
            }
        }

        // Configurar matrices de transformación
        FbxAMatrix linkMatrix = boneNode->EvaluateGlobalTransform();
        FbxAMatrix meshMatrix = meshNode->EvaluateGlobalTransform();

        // Bind pose matrix (offset matrix)
        FbxAMatrix bindPoseMatrix = MatrixConverter::ConvertMatrix_LH_to_RH(bone.offsetMatrix);

        cluster->SetTransformMatrix(meshMatrix);
        cluster->SetTransformLinkMatrix(linkMatrix);

        skin->AddCluster(cluster);
    }

    // Agregar skin al mesh
    fbxMesh->AddDeformer(skin);
}

FbxNode* FBXExporter::CreateBone(
    const string& boneName,
    const D3DXMATRIX& transformMatrix,
    FbxNode* parentNode)
{
    // Crear skeleton attribute
    FbxSkeleton* skeletonAttribute = FbxSkeleton::Create(m_pScene, boneName.c_str());
    skeletonAttribute->SetSkeletonType(FbxSkeleton::eLimbNode);

    // Crear nodo
    FbxNode* boneNode = FbxNode::Create(m_pScene, boneName.c_str());
    boneNode->SetNodeAttribute(skeletonAttribute);

    // Aplicar transformación
    FbxAMatrix transform = MatrixConverter::ConvertMatrix_LH_to_RH(transformMatrix);
    boneNode->LclTranslation.Set(transform.GetT());
    boneNode->LclRotation.Set(transform.GetR());
    boneNode->LclScaling.Set(transform.GetS());

    // Agregar al padre
    parentNode->AddChild(boneNode);

    // Registrar en mapa
    m_BoneNodeMap[boneName] = boneNode;

    return boneNode;
}

// ============================================================================
// Exportación de Animaciones
// ============================================================================

void FBXExporter::ExportAnimations(const vector<AnimationClip>& animations)
{
    for (const AnimationClip& clip : animations)
    {
        ExportAnimationClip(clip);
    }
}

// ============================================================================
// EXPORTACIÓN DE CLIP DE ANIMACIÓN
// ============================================================================
// Esta función exporta un clip de animación completo al FBX.
// Crea un AnimStack (contenedor de animación) y un AnimLayer (capa de animación)
// luego agrega curvas de animación para cada hueso que tiene keyframes.
//
// FBX organiza las animaciones en una jerarquía:
// AnimStack (clip completo)
//   └─ AnimLayer (capa, permite mezclar múltiples animaciones)
//       └─ AnimCurves (curvas para cada propiedad: TX, TY, TZ, RX, RY, RZ, SX, SY, SZ)
// ============================================================================
void FBXExporter::ExportAnimationClip(const AnimationClip& clip)
{
    // ========================================================================
    // PASO 1: Crear estructura de animación en FBX
    // ========================================================================

    // AnimStack: Contenedor principal del clip de animación
    // Cada AnimStack representa un clip completo (ej: "Walk", "Run", "Jump")
    FbxAnimStack* animStack = FbxAnimStack::Create(m_pScene, clip.name.c_str());

    // AnimLayer: Capa de animación dentro del stack
    // FBX permite múltiples layers para mezclar animaciones (additive blending)
    // Por simplicidad, usamos una sola capa llamada "BaseLayer"
    FbxAnimLayer* animLayer = FbxAnimLayer::Create(m_pScene, "BaseLayer");
    animStack->AddMember(animLayer);

    // ========================================================================
    // PASO 2: Configurar tiempo de inicio y fin del clip
    // ========================================================================

    FbxTime startTime, endTime;
    startTime.SetSecondDouble(0.0);                    // Inicio en tiempo 0
    endTime.SetSecondDouble(clip.duration);            // Fin según duración del clip

    // FbxTimeSpan define el rango de tiempo que cubre esta animación
    FbxTimeSpan timeSpan(startTime, endTime);
    animStack->SetLocalTimeSpan(timeSpan);

    // ========================================================================
    // PASO 3: Exportar tracks de animación (uno por cada hueso animado)
    // ========================================================================

    // Cada AnimationTrack contiene los keyframes para un hueso específico
    for (const AnimationTrack& track : clip.tracks)
    {
        // ====================================================================
        // Buscar el nodo FBX correspondiente a este hueso
        // ====================================================================
        // m_BoneNodeMap es un mapa (string -> FbxNode*) que fue construido
        // durante la exportación de la jerarquía de frames.
        // Contiene todos los nodos que pueden ser animados.

        auto it = m_BoneNodeMap.find(track.boneName);
        if (it == m_BoneNodeMap.end())
            continue;  // Hueso no encontrado, saltar este track

        FbxNode* boneNode = it->second;

        // ====================================================================
        // Crear 9 curvas de animación: Translation (X,Y,Z), Rotation (X,Y,Z), Scale (X,Y,Z)
        // ====================================================================
        // FBX almacena animaciones como curvas separadas para cada componente.
        // Cada curva contiene keyframes (tiempo, valor).
        //
        // GetCurve() obtiene o crea la curva para un componente específico:
        //   - animLayer: capa donde se almacena la curva
        //   - COMPONENT_X/Y/Z: componente específico (X, Y o Z)
        //   - true: crear la curva si no existe

        // Curvas de TRASLACIÓN (posición del hueso en el espacio)
        FbxAnimCurve* curveTX = boneNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
        FbxAnimCurve* curveTY = boneNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
        FbxAnimCurve* curveTZ = boneNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

        // Curvas de ROTACIÓN (orientación del hueso, en grados Euler)
        FbxAnimCurve* curveRX = boneNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
        FbxAnimCurve* curveRY = boneNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
        FbxAnimCurve* curveRZ = boneNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

        // Curvas de ESCALA (tamaño del hueso)
        FbxAnimCurve* curveSX = boneNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
        FbxAnimCurve* curveSY = boneNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
        FbxAnimCurve* curveSZ = boneNode->LclScaling.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

        // ====================================================================
        // OPTIMIZACIÓN: Iniciar modificación de TODAS las curvas una sola vez
        // ====================================================================
        // Esto es mucho más eficiente que llamar Begin/End para cada keyframe
        curveTX->KeyModifyBegin();
        curveTY->KeyModifyBegin();
        curveTZ->KeyModifyBegin();
        curveRX->KeyModifyBegin();
        curveRY->KeyModifyBegin();
        curveRZ->KeyModifyBegin();
        curveSX->KeyModifyBegin();
        curveSY->KeyModifyBegin();
        curveSZ->KeyModifyBegin();

        // ====================================================================
        // Agregar keyframes a las curvas
        // ====================================================================
        // Iterar sobre cada keyframe en este track
        for (const AnimationKey& key : track.keys)
        {
            // ================================================================
            // Convertir tiempo del keyframe a FbxTime
            // ================================================================
            FbxTime keyTime;
            keyTime.SetSecondDouble(key.time);  // Tiempo en segundos

            // ================================================================
            // CONVERSIÓN CRÍTICA: DirectX (LH) → FBX (RH)
            // ================================================================
            // DirectX usa Left-Handed (Z hacia adelante)
            // FBX usa Right-Handed (Z hacia atrás)
            // Debemos convertir TODAS las transformaciones

            // Convertir POSICIÓN (invierte Z)
            FbxVector4 pos = MatrixConverter::ConvertPosition_LH_to_RH(key.translation);

            // Convertir ROTACIÓN (quaternion → negar X,Y)
            FbxQuaternion rot = MatrixConverter::ConvertQuaternion_LH_to_RH(key.rotation);

            // FBX almacena rotaciones como Euler angles (X,Y,Z en grados)
            // Descomponer quaternion a ángulos de Euler
            FbxVector4 euler = rot.DecomposeSphericalXYZ();  // Retorna radianes

            // Convertir ESCALA (sin cambios entre LH y RH)
            FbxVector4 scale = MatrixConverter::ConvertScale(key.scale);

            // ================================================================
            // AGREGAR KEYFRAMES DE TRASLACIÓN (TX, TY, TZ)
            // ================================================================

            // Keyframe para Translation X
            int keyIndexTX = curveTX->KeyAdd(keyTime);
            curveTX->KeySet(keyIndexTX, keyTime, (float)pos[0]);

            // Keyframe para Translation Y
            int keyIndexTY = curveTY->KeyAdd(keyTime);
            curveTY->KeySet(keyIndexTY, keyTime, (float)pos[1]);

            // Keyframe para Translation Z
            int keyIndexTZ = curveTZ->KeyAdd(keyTime);
            curveTZ->KeySet(keyIndexTZ, keyTime, (float)pos[2]);

            // ================================================================
            // AGREGAR KEYFRAMES DE ROTACIÓN (RX, RY, RZ)
            // ================================================================
            // IMPORTANTE: FBX espera ángulos de Euler en GRADOS, no radianes
            // DecomposeSphericalXYZ() retorna radianes, debemos convertir

            const double RAD_TO_DEG = 180.0 / 3.14159265358979323846;

            // Keyframe para Rotation X (en grados)
            int keyIndexRX = curveRX->KeyAdd(keyTime);
            curveRX->KeySet(keyIndexRX, keyTime, (float)(euler[0] * RAD_TO_DEG));

            // Keyframe para Rotation Y (en grados)
            int keyIndexRY = curveRY->KeyAdd(keyTime);
            curveRY->KeySet(keyIndexRY, keyTime, (float)(euler[1] * RAD_TO_DEG));

            // Keyframe para Rotation Z (en grados)
            int keyIndexRZ = curveRZ->KeyAdd(keyTime);
            curveRZ->KeySet(keyIndexRZ, keyTime, (float)(euler[2] * RAD_TO_DEG));

            // ================================================================
            // AGREGAR KEYFRAMES DE ESCALA (SX, SY, SZ)
            // ================================================================
            int keyIndexSX = curveSX->KeyAdd(keyTime);
            curveSX->KeySet(keyIndexSX, keyTime, (float)scale[0]);

            int keyIndexSY = curveSY->KeyAdd(keyTime);
            curveSY->KeySet(keyIndexSY, keyTime, (float)scale[1]);

            int keyIndexSZ = curveSZ->KeyAdd(keyTime);
            curveSZ->KeySet(keyIndexSZ, keyTime, (float)scale[2]);
        }

        // ====================================================================
        // OPTIMIZACIÓN: Finalizar modificación de TODAS las curvas una vez
        // ====================================================================
        curveTX->KeyModifyEnd();
        curveTY->KeyModifyEnd();
        curveTZ->KeyModifyEnd();
        curveRX->KeyModifyEnd();
        curveRY->KeyModifyEnd();
        curveRZ->KeyModifyEnd();
        curveSX->KeyModifyEnd();
        curveSY->KeyModifyEnd();
        curveSZ->KeyModifyEnd();
    }
}

// ============================================================================
// Configuración de Propiedades
// ============================================================================

void FBXExporter::SetupSceneProperties()
{
    FbxDocumentInfo* sceneInfo = FbxDocumentInfo::Create(m_pManager, "SceneInfo");
    sceneInfo->mTitle = "Converted from DirectX .X";
    sceneInfo->mSubject = "DirectX to FBX Conversion";
    sceneInfo->mAuthor = "XtoFBX Converter";
    sceneInfo->mComment = "Automatically converted using custom converter";

    m_pScene->SetSceneInfo(sceneInfo);
}

void FBXExporter::SetupCoordinateSystem()
{
    // FBX usa Right-Handed por defecto, Y-Up
    FbxAxisSystem axisSystem;

    if (m_Options.targetCoordSystem == CoordinateSystem::RIGHT_HANDED)
    {
        if (m_Options.upAxis == UpAxis::Y_AXIS)
        {
            // Right-Handed, Y-Up (Maya, FBX default)
            axisSystem = FbxAxisSystem::MayaYUp;
        }
        else if (m_Options.upAxis == UpAxis::Z_AXIS)
        {
            // Right-Handed, Z-Up (3ds Max, Blender)
            axisSystem = FbxAxisSystem::Max;
        }
    }
    else
    {
        // Left-Handed (DirectX, Unreal)
        axisSystem = FbxAxisSystem::DirectX;
    }

    axisSystem.ConvertScene(m_pScene);

    // Configurar unidades (centímetros por defecto)
    FbxSystemUnit sceneSystemUnit = FbxSystemUnit::cm;
    if (m_Options.scale != 1.0f)
    {
        sceneSystemUnit = FbxSystemUnit(m_Options.scale * 100.0);  // Convertir a cm
    }
    sceneSystemUnit.ConvertScene(m_pScene);
}

// ============================================================================
// Helpers
// ============================================================================

FrameData* FBXExporter::FindFrameByName(FrameData* root, const string& name)
{
    if (!root)
        return nullptr;

    if (root->name == name)
        return root;

    for (FrameData* child : root->children)
    {
        FrameData* result = FindFrameByName(child, name);
        if (result)
            return result;
    }

    return nullptr;
}

string FBXExporter::CopyTexture(const string& textureFilename)
{
    if (!Utils::FileExists(textureFilename))
        return textureFilename;

    try
    {
        fs::path srcPath(textureFilename);
        fs::path dstDir = fs::path(m_Options.outputFile).parent_path() / "textures";

        // Crear directorio de texturas
        fs::create_directories(dstDir);

        fs::path dstPath = dstDir / srcPath.filename();

        // Copiar archivo
        fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing);

        return dstPath.string();
    }
    catch (const fs::filesystem_error& e)
    {
        Utils::LogWarning("Failed to copy texture: " + string(e.what()));
        return textureFilename;
    }
}
