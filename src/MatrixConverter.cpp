#include "MatrixConverter.h"

// Matriz de conversión estática: Invierte Z
// FBX SDK 2020.3.7: FbxAMatrix constructor changed - use SetIdentity and SetRow
FbxAMatrix MatrixConverter::s_ConversionMatrix_LH_to_RH;
bool MatrixConverter::s_ConversionMatrixInitialized = false;

void MatrixConverter::InitializeConversionMatrix()
{
    if (!s_ConversionMatrixInitialized)
    {
        s_ConversionMatrix_LH_to_RH.SetIdentity();
        s_ConversionMatrix_LH_to_RH.SetRow(0, FbxVector4(1.0, 0.0, 0.0, 0.0));
        s_ConversionMatrix_LH_to_RH.SetRow(1, FbxVector4(0.0, 1.0, 0.0, 0.0));
        s_ConversionMatrix_LH_to_RH.SetRow(2, FbxVector4(0.0, 0.0, -1.0, 0.0));
        s_ConversionMatrix_LH_to_RH.SetRow(3, FbxVector4(0.0, 0.0, 0.0, 1.0));
        s_ConversionMatrixInitialized = true;
    }
}

// ============================================================================
// Conversión de Matriz 4x4: DirectX (LH) -> FBX (RH)
// ============================================================================

// ============================================================================
// CONVERSIÓN DE MATRIZ 4x4: DirectX (LH) → FBX (RH)
// ============================================================================
// Esta es la función MÁS IMPORTANTE del convertidor.
// Convierte matrices de transformación de DirectX (Left-Handed) a FBX (Right-Handed).
//
// DirectX LH: X=Derecha, Y=Arriba, Z=Adelante
// FBX RH:     X=Derecha, Y=Arriba, Z=Atrás (Z invertido)
//
// Estrategia:
// 1. Descomponer matriz en Translation, Rotation, Scale (TRS)
// 2. Convertir cada componente individualmente
// 3. Recomponer matriz FBX con componentes convertidos
//
// ¿Por qué no multiplicar directamente por matriz de conversión?
// Porque la simple multiplicación no preserva correctamente las rotaciones
// cuando hay escalas no uniformes. La descomposición TRS es más robusta.
// ============================================================================
FbxAMatrix MatrixConverter::ConvertMatrix_LH_to_RH(const D3DXMATRIX& dxMatrix)
{
    // Asegurar que la matriz de conversión esté inicializada
    InitializeConversionMatrix();

    // ========================================================================
    // PASO 1: Descomponer la matriz DirectX en componentes TRS
    // ========================================================================
    // Una matriz 4x4 de transformación se puede descomponer en:
    //   T = Translation (traslación/posición)
    //   R = Rotation (rotación/orientación)
    //   S = Scale (escala/tamaño)

    D3DXVECTOR3 translation, scale;
    D3DXQUATERNION rotation;
    DecomposeMatrix(dxMatrix, translation, rotation, scale);

    // ========================================================================
    // PASO 2: Convertir cada componente de LH a RH
    // ========================================================================
    // Cada componente requiere una conversión específica:
    //   Translation: invertir Z
    //   Rotation: negar X e Y del quaternion
    //   Scale: sin cambios

    FbxVector4 fbxTranslation = ConvertPosition_LH_to_RH(translation);
    FbxQuaternion fbxRotation = ConvertQuaternion_LH_to_RH(rotation);
    FbxVector4 fbxScale = ConvertScale(scale);

    // ========================================================================
    // PASO 3: Reconstruir matriz FBX desde componentes convertidos
    // ========================================================================
    // FbxAMatrix puede construirse desde componentes TRS

    FbxAMatrix result;
    result.SetT(fbxTranslation);   // Establecer traslación
    result.SetQ(fbxRotation);      // Establecer rotación (quaternion)
    result.SetS(fbxScale);         // Establecer escala

    return result;
}

FbxAMatrix MatrixConverter::D3DMatrixToFbxAMatrix(const D3DXMATRIX& dxMatrix)
{
    // Conversión directa sin cambiar el sistema de coordenadas
    // DirectX: row-major, FBX: row-major también en FbxAMatrix

    FbxAMatrix fbxMatrix;

    // DirectX y FBX ambos usan row-major en sus representaciones
    double mat[4][4];
    for (int row = 0; row < 4; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            mat[row][col] = dxMatrix.m[row][col];
        }
    }

    fbxMatrix.SetRow(0, FbxVector4(mat[0][0], mat[0][1], mat[0][2], mat[0][3]));
    fbxMatrix.SetRow(1, FbxVector4(mat[1][0], mat[1][1], mat[1][2], mat[1][3]));
    fbxMatrix.SetRow(2, FbxVector4(mat[2][0], mat[2][1], mat[2][2], mat[2][3]));
    fbxMatrix.SetRow(3, FbxVector4(mat[3][0], mat[3][1], mat[3][2], mat[3][3]));

    return fbxMatrix;
}

// ============================================================================
// Conversión de Vectores
// ============================================================================

// ============================================================================
// CONVERSIÓN DE POSICIÓN: DirectX LH → FBX RH
// ============================================================================
// Convierte un punto en el espacio 3D de Left-Handed a Right-Handed.
//
// Visualización:
//   DirectX LH:        FBX RH:
//      Y                  Y
//      |                  |
//      |                  |
//      +---X              +---X
//     /                    \
//    Z (adelante)           Z (atrás)
//
// Conversión: (X, Y, Z) → (X, Y, -Z)
// Solo invertimos el eje Z para cambiar la "dirección hacia adelante"
// ============================================================================
FbxVector4 MatrixConverter::ConvertPosition_LH_to_RH(const D3DXVECTOR3& dxPos)
{
    return FbxVector4(
        dxPos.x,   // X permanece igual (derecha en ambos sistemas)
        dxPos.y,   // Y permanece igual (arriba en ambos sistemas)
        -dxPos.z,  // Z se invierte (adelante→atrás)
        1.0        // w = 1.0 indica que es un PUNTO (no un vector direccional)
    );
}

// ============================================================================
// CONVERSIÓN DE NORMAL: DirectX LH → FBX RH
// ============================================================================
// Las normales son vectores direccionales que indican orientación de superficies.
// Se convierten igual que las posiciones (invertir Z) pero con w=0.
//
// w=0 vs w=1:
//   w=1: Punto en el espacio (afectado por traslaciones)
//   w=0: Vector direccional (NO afectado por traslaciones)
// ============================================================================
FbxVector4 MatrixConverter::ConvertNormal_LH_to_RH(const D3DXVECTOR3& dxNormal)
{
    return FbxVector4(
        dxNormal.x,   // X permanece igual
        dxNormal.y,   // Y permanece igual
        -dxNormal.z,  // Z se invierte
        0.0           // w = 0.0 indica que es un VECTOR DIRECCIONAL
    );
}

// ============================================================================
// CONVERSIÓN DE ESCALA: DirectX LH → FBX RH
// ============================================================================
// La escala es invariante entre sistemas de coordenadas.
// Un objeto con escala (2,3,4) tiene el mismo tamaño en ambos sistemas.
// ============================================================================
FbxVector4 MatrixConverter::ConvertScale(const D3DXVECTOR3& dxScale)
{
    // La escala NO cambia entre sistemas de coordenadas
    return FbxVector4(dxScale.x, dxScale.y, dxScale.z, 1.0);
}

// ============================================================================
// Conversión de Quaternions
// ============================================================================

// ============================================================================
// CONVERSIÓN DE QUATERNION: DirectX LH → FBX RH
// ============================================================================
// Los quaternions son una forma matemática de representar rotaciones 3D.
// Un quaternion tiene 4 componentes: (x, y, z, w)
//   - (x, y, z) = parte vectorial (eje de rotación)
//   - w = parte escalar (cantidad de rotación)
//
// ¿Por qué usar quaternions?
//   - No sufren de "gimbal lock" (problema de los ángulos de Euler)
//   - Interpolación suave entre rotaciones (SLERP)
//   - Matemáticamente estables
//
// Conversión LH → RH:
//   Para cambiar de sistema de coordenadas, negamos X e Y.
//   Esto refleja el quaternion a través del plano XY, cambiando efectivamente
//   la dirección del eje Z.
//
// Matemática:
//   Si q = (x, y, z, w) representa una rotación en LH,
//   entonces q' = (-x, -y, z, w) representa la misma rotación en RH.
// ============================================================================
FbxQuaternion MatrixConverter::ConvertQuaternion_LH_to_RH(const D3DXQUATERNION& dxQuat)
{
    // Negar componentes X e Y para reflejar el cambio de sistema de coordenadas
    FbxQuaternion fbxQuat(
        -dxQuat.x,  // Negar X (refleja alrededor del eje YZ)
        -dxQuat.y,  // Negar Y (refleja alrededor del eje XZ)
        dxQuat.z,   // Mantener Z (eje que invertimos en el espacio)
        dxQuat.w    // Mantener W (magnitud de la rotación)
    );

    // Normalizar el quaternion para asegurar que represente una rotación válida
    // Un quaternion debe tener magnitud = 1.0 para ser una rotación pura
    // |q| = sqrt(x² + y² + z² + w²) = 1.0
    fbxQuat.Normalize();

    return fbxQuat;
}

// ============================================================================
// Descomposición de Matrices
// ============================================================================

// ============================================================================
// DESCOMPOSICIÓN DE MATRIZ 4x4 EN TRS (Translation, Rotation, Scale)
// ============================================================================
// Una matriz de transformación 4x4 combina traslación, rotación y escala.
// Esta función la descompone en sus componentes individuales.
//
// Estructura de una matriz 4x4:
//   [ Rx*Sx  Ry*Sx  Rz*Sx  Tx ]     R = Rotación (3x3)
//   [ Rx*Sy  Ry*Sy  Rz*Sy  Ty ]     S = Escala (diagonal)
//   [ Rx*Sz  Ry*Sz  Rz*Sz  Tz ]     T = Traslación (última columna)
//   [   0      0      0     1  ]
//
// Proceso de descomposición:
//   1. Extraer T (última columna) → trivial
//   2. Extraer S (longitud de vectores columna) → cálculo de magnitud
//   3. Extraer R (matriz normalizada sin escala) → división
//   4. Convertir R a quaternion → algoritmo de conversión
// ============================================================================
void MatrixConverter::DecomposeMatrix(
    const D3DXMATRIX& matrix,
    D3DXVECTOR3& translation,
    D3DXQUATERNION& rotation,
    D3DXVECTOR3& scale)
{
    // ========================================================================
    // PASO 1: Extraer TRASLACIÓN (última columna de la matriz)
    // ========================================================================
    // En una matriz 4x4, la traslación está en (m41, m42, m43)
    //   [ ..  ..  ..  Tx ]
    //   [ ..  ..  ..  Ty ]
    //   [ ..  ..  ..  Tz ]
    //   [  0   0   0   1 ]

    translation = ExtractTranslation(matrix);

    // ========================================================================
    // PASO 2: Extraer ESCALA (longitud de vectores columna)
    // ========================================================================
    // Cada columna de la matriz 3x3 superior izquierda es un vector de base.
    // La longitud de cada vector es el factor de escala en ese eje.
    //
    // Ejemplo: Si la primera columna es (2, 0, 0), la escala en X es 2.

    scale = ExtractScale(matrix);

    // ========================================================================
    // PASO 3: Construir matriz de rotación PURA (sin escala ni traslación)
    // ========================================================================
    // Necesitamos aislar solo la rotación, eliminando escala y traslación.

    D3DXMATRIX rotationMatrix = matrix;

    // Eliminar traslación (poner última columna en 0)
    rotationMatrix._41 = 0.0f;  // Tx = 0
    rotationMatrix._42 = 0.0f;  // Ty = 0
    rotationMatrix._43 = 0.0f;  // Tz = 0

    // Eliminar escala dividiendo cada COLUMNA por su longitud (factor de escala)
    // Esto normaliza los vectores de base a longitud 1.0

    // Normalizar columna X (primera columna)
    if (scale.x != 0.0f)  // Evitar división por cero
    {
        rotationMatrix._11 /= scale.x;
        rotationMatrix._12 /= scale.x;
        rotationMatrix._13 /= scale.x;
    }

    // Normalizar columna Y (segunda columna)
    if (scale.y != 0.0f)
    {
        rotationMatrix._21 /= scale.y;
        rotationMatrix._22 /= scale.y;
        rotationMatrix._23 /= scale.y;
    }

    // Normalizar columna Z (tercera columna)
    if (scale.z != 0.0f)
    {
        rotationMatrix._31 /= scale.z;
        rotationMatrix._32 /= scale.z;
        rotationMatrix._33 /= scale.z;
    }

    // ========================================================================
    // PASO 4: Convertir matriz de rotación a quaternion
    // ========================================================================
    // DirectX proporciona una función para convertir matriz 3x3 → quaternion
    // Usa el algoritmo de Shepperd's method para estabilidad numérica

    D3DXQuaternionRotationMatrix(&rotation, &rotationMatrix);

    // Normalizar para asegurar que es un quaternion unitario válido
    rotation = NormalizeQuaternion(rotation);
}

D3DXVECTOR3 MatrixConverter::ExtractTranslation(const D3DXMATRIX& matrix)
{
    // La traslación está en la última columna (índices 41, 42, 43)
    return D3DXVECTOR3(matrix._41, matrix._42, matrix._43);
}

D3DXVECTOR3 MatrixConverter::ExtractScale(const D3DXMATRIX& matrix)
{
    // La escala es la longitud de cada vector de la base (columnas)
    D3DXVECTOR3 scaleX(matrix._11, matrix._12, matrix._13);
    D3DXVECTOR3 scaleY(matrix._21, matrix._22, matrix._23);
    D3DXVECTOR3 scaleZ(matrix._31, matrix._32, matrix._33);

    float sx = D3DXVec3Length(&scaleX);
    float sy = D3DXVec3Length(&scaleY);
    float sz = D3DXVec3Length(&scaleZ);

    return D3DXVECTOR3(sx, sy, sz);
}

D3DXQUATERNION MatrixConverter::ExtractRotation(const D3DXMATRIX& matrix)
{
    D3DXVECTOR3 translation, scale;
    D3DXQUATERNION rotation;
    DecomposeMatrix(matrix, translation, rotation, scale);
    return rotation;
}

D3DXQUATERNION MatrixConverter::NormalizeQuaternion(const D3DXQUATERNION& q)
{
    D3DXQUATERNION result = q;
    D3DXQuaternionNormalize(&result, &q);
    return result;
}

// ============================================================================
// Creación de Matrices FBX
// ============================================================================

FbxAMatrix MatrixConverter::CreateTransformMatrix(
    const FbxVector4& translation,
    const FbxVector4& rotation,
    const FbxVector4& scale)
{
    FbxAMatrix matrix;

    matrix.SetT(translation);
    matrix.SetR(rotation);  // Euler angles en grados
    matrix.SetS(scale);

    return matrix;
}

// ============================================================================
// Funciones Auxiliares
// ============================================================================

FbxVector4 MatrixConverter::ApplyGlobalScale(const FbxVector4& position, float scale)
{
    return FbxVector4(
        position[0] * scale,
        position[1] * scale,
        position[2] * scale,
        position[3]
    );
}

FbxAMatrix MatrixConverter::ConvertMatrixWithOptions(
    const D3DXMATRIX& matrix,
    const ConversionOptions& options)
{
    FbxAMatrix result;

    // Convertir basado en el sistema de coordenadas objetivo
    if (options.targetCoordSystem == CoordinateSystem::RIGHT_HANDED)
    {
        result = ConvertMatrix_LH_to_RH(matrix);
    }
    else
    {
        result = D3DMatrixToFbxAMatrix(matrix);
    }

    // Aplicar escala global si es necesario
    if (options.scale != 1.0f)
    {
        FbxVector4 translation = result.GetT();
        translation = ApplyGlobalScale(translation, options.scale);
        result.SetT(translation);
    }

    // Ajustar eje vertical si es necesario
    if (options.upAxis == UpAxis::Z_AXIS)
    {
        // Rotar de Y-Up a Z-Up
        FbxAMatrix rotationMatrix;
        FbxVector4 rotation(-90.0, 0.0, 0.0);  // Rotar -90° en X
        rotationMatrix.SetR(rotation);
        result = rotationMatrix * result;
    }

    return result;
}
