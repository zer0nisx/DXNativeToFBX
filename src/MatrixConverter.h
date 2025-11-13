#pragma once

#ifndef MATRIX_CONVERTER_H
#define MATRIX_CONVERTER_H

#include "../include/Common.h"

/**
 * @class MatrixConverter
 * @brief Maneja la conversión de matrices entre sistemas de coordenadas
 *
 * DirectX usa Left-Handed coordinate system:
 *   X = Right, Y = Up, Z = Forward
 *
 * FBX/Maya/OpenGL usan Right-Handed coordinate system:
 *   X = Right, Y = Up, Z = Backward (o X = Right, Y = Forward, Z = Up dependiendo del sistema)
 *
 * La conversión principal invierte el eje Z y ajusta la rotación
 */
class MatrixConverter
{
public:
    /**
     * Convertir matriz 4x4 de DirectX (Left-Handed) a FBX (Right-Handed)
     * @param dxMatrix Matriz de DirectX
     * @return FbxAMatrix Matriz convertida para FBX
     */
    static FbxAMatrix ConvertMatrix_LH_to_RH(const D3DXMATRIX& dxMatrix);

    /**
     * Convertir matriz 4x4 de DirectX a FBX sin cambiar sistema de coordenadas
     * @param dxMatrix Matriz de DirectX
     * @return FbxAMatrix Matriz FBX equivalente
     */
    static FbxAMatrix D3DMatrixToFbxAMatrix(const D3DXMATRIX& dxMatrix);

    /**
     * Convertir vector de posición de DirectX a FBX
     * @param dxPos Posición en DirectX
     * @return FbxVector4 Posición convertida
     */
    static FbxVector4 ConvertPosition_LH_to_RH(const D3DXVECTOR3& dxPos);

    /**
     * Convertir vector normal de DirectX a FBX
     * @param dxNormal Normal en DirectX
     * @return FbxVector4 Normal convertida
     */
    static FbxVector4 ConvertNormal_LH_to_RH(const D3DXVECTOR3& dxNormal);

    /**
     * Convertir quaternion de DirectX a FBX
     * @param dxQuat Quaternion de DirectX
     * @return FbxQuaternion Quaternion convertido
     */
    static FbxQuaternion ConvertQuaternion_LH_to_RH(const D3DXQUATERNION& dxQuat);

    /**
     * Convertir escala (sin cambios, pero por consistencia)
     * @param dxScale Escala en DirectX
     * @return FbxVector4 Escala para FBX
     */
    static FbxVector4 ConvertScale(const D3DXVECTOR3& dxScale);

    /**
     * Descomponer matriz D3DXMATRIX en Translation, Rotation, Scale
     * @param matrix Matriz a descomponer
     * @param translation [out] Traslación
     * @param rotation [out] Rotación (quaternion)
     * @param scale [out] Escala
     */
    static void DecomposeMatrix(
        const D3DXMATRIX& matrix,
        D3DXVECTOR3& translation,
        D3DXQUATERNION& rotation,
        D3DXVECTOR3& scale
    );

    /**
     * Extraer rotación de una matriz
     * @param matrix Matriz
     * @return D3DXQUATERNION Rotación como quaternion
     */
    static D3DXQUATERNION ExtractRotation(const D3DXMATRIX& matrix);

    /**
     * Extraer escala de una matriz
     * @param matrix Matriz
     * @return D3DXVECTOR3 Escala en cada eje
     */
    static D3DXVECTOR3 ExtractScale(const D3DXMATRIX& matrix);

    /**
     * Extraer traslación de una matriz
     * @param matrix Matriz
     * @return D3DXVECTOR3 Traslación
     */
    static D3DXVECTOR3 ExtractTranslation(const D3DXMATRIX& matrix);

    /**
     * Crear matriz de transformación FBX desde componentes
     * @param translation Traslación
     * @param rotation Rotación (Euler angles en grados)
     * @param scale Escala
     * @return FbxAMatrix Matriz de transformación
     */
    static FbxAMatrix CreateTransformMatrix(
        const FbxVector4& translation,
        const FbxVector4& rotation,
        const FbxVector4& scale
    );

    /**
     * Aplicar escala global a una posición
     * @param position Posición original
     * @param scale Factor de escala
     * @return FbxVector4 Posición escalada
     */
    static FbxVector4 ApplyGlobalScale(const FbxVector4& position, float scale);

    /**
     * Convertir sistema de coordenadas basado en opciones
     * @param matrix Matriz original
     * @param options Opciones de conversión
     * @return FbxAMatrix Matriz convertida
     */
    static FbxAMatrix ConvertMatrixWithOptions(
        const D3DXMATRIX& matrix,
        const ConversionOptions& options
    );

private:
    /**
     * Matriz de conversión de Left-Handed a Right-Handed
     * Invierte el eje Z: [1,0,0,0; 0,1,0,0; 0,0,-1,0; 0,0,0,1]
     */
    static FbxAMatrix s_ConversionMatrix_LH_to_RH;
    static bool s_ConversionMatrixInitialized;
    static void InitializeConversionMatrix();

    /**
     * Helper: Normalizar quaternion
     */
    static D3DXQUATERNION NormalizeQuaternion(const D3DXQUATERNION& q);
};

#endif // MATRIX_CONVERTER_H
