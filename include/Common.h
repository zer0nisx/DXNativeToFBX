#pragma once

#ifndef COMMON_H
#define COMMON_H

// Platform
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Standard Library
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <fstream>
#include <sstream>

// DirectX 9
#include <d3d9.h>
#include <d3dx9.h>
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

// FBX SDK
#include <fbxsdk.h>
#ifdef _DEBUG
    #pragma comment(lib, "libfbxsdk-md.lib")
#else
    #pragma comment(lib, "libfbxsdk-md.lib")
#endif

// Namespace
using namespace std;

// Constants
#define MAX_BONE_INFLUENCES 4
#define EPSILON 0.0001f

// Coordinate System
enum class CoordinateSystem
{
	LEFT_HANDED,   // DirectX
	RIGHT_HANDED   // FBX, OpenGL, Maya
};

enum class UpAxis
{
	X_AXIS,
	Y_AXIS,
	Z_AXIS
};

// Conversion Options
struct ConversionOptions
{
	string inputFile;
	string outputFile;

	CoordinateSystem targetCoordSystem = CoordinateSystem::RIGHT_HANDED;
	UpAxis upAxis = UpAxis::Y_AXIS;
	float scale = 1.0f;

	bool exportTextures = true;
	bool mergeMaterials = false;
	bool triangulate = true;
	bool verbose = false;

    int fbxVersion = -1; // FBX version (-1 = auto-detect, or use FbxIOPluginRegistry format ID)

	// Opciones de animación
	double targetFPS = 30.0; // FPS objetivo para la exportación (30 o 60 recomendado)
	bool resampleAnimation = true; // Resamplear animación al FPS objetivo

	ConversionOptions() {}
};

// Vertex Structure (compatible con DirectX .X)
struct Vertex
{
	D3DXVECTOR3 position;
	D3DXVECTOR3 normal;
	D3DXVECTOR2 texCoord;

	// Skin weights (hasta 4 influencias)
	DWORD boneIndices[MAX_BONE_INFLUENCES];
	float boneWeights[MAX_BONE_INFLUENCES];

	Vertex()
	{
		position = D3DXVECTOR3(0, 0, 0);
		normal = D3DXVECTOR3(0, 1, 0);
		texCoord = D3DXVECTOR2(0, 0);

		for (int i = 0; i < MAX_BONE_INFLUENCES; i++)
		{
			boneIndices[i] = 0;
			boneWeights[i] = 0.0f;
		}
	}
};

// Material Structure
struct MaterialData
{
	string name;
	D3DMATERIAL9 material;
	string textureFilename;

	MaterialData()
	{
		name = "DefaultMaterial";
		ZeroMemory(&material, sizeof(D3DMATERIAL9));
		material.Diffuse.r = material.Diffuse.g = material.Diffuse.b = 0.8f;
		material.Diffuse.a = 1.0f;
		material.Ambient = material.Diffuse;
		material.Specular.r = material.Specular.g = material.Specular.b = 1.0f;
		material.Specular.a = 1.0f;
		material.Power = 32.0f;
	}
};

// Bone Structure
struct BoneData
{
	string name;
	D3DXMATRIX offsetMatrix;
	D3DXMATRIX transformMatrix;
	int parentIndex;

	BoneData()
	{
		name = "";
		D3DXMatrixIdentity(&offsetMatrix);
		D3DXMatrixIdentity(&transformMatrix);
		parentIndex = -1;
	}
};

// Animation Keyframe
struct AnimationKey
{
	double time;
	D3DXVECTOR3 translation;
	D3DXQUATERNION rotation;
	D3DXVECTOR3 scale;

	AnimationKey()
	{
		time = 0.0;
		translation = D3DXVECTOR3(0, 0, 0);
		rotation = D3DXQUATERNION(0, 0, 0, 1);
		scale = D3DXVECTOR3(1, 1, 1);
	}
};

// Animation Track (por hueso)
struct AnimationTrack
{
	string boneName;
	vector<AnimationKey> keys;

	AnimationTrack() {}
};

// Animation Clip
struct AnimationClip
{
	string name;
	double duration;
	double ticksPerSecond;
	vector<AnimationTrack> tracks;

	AnimationClip()
	{
		name = "Take001";
		duration = 0.0;
		ticksPerSecond = 30.0; // 30 FPS por defecto
	}
};

// Mesh Data
struct MeshData
{
	string name;
	vector<Vertex> vertices;
	vector<DWORD> indices;
	vector<DWORD> materialIndices; // índice de material por triángulo

	bool hasSkinning;
	vector<BoneData> bones;

	MeshData()
	{
		name = "Mesh";
		hasSkinning = false;
	}
};

// Frame Hierarchy (equivalente a D3DXFRAME)
struct FrameData
{
	string name;
	D3DXMATRIX transformMatrix;
	D3DXMATRIX combinedMatrix;

	FrameData* parent;
	vector<FrameData*> children;
	vector<MeshData*> meshes;

	FrameData()
	{
		name = "";
		D3DXMatrixIdentity(&transformMatrix);
		D3DXMatrixIdentity(&combinedMatrix);
		parent = nullptr;
	}

	~FrameData()
	{
		for (auto mesh : meshes)
			delete mesh;
		for (auto child : children)
			delete child;
	}
};

// Scene Data (todo el contenido del archivo .X)
struct SceneData
{
	FrameData* rootFrame;
	vector<MaterialData> materials;
	vector<AnimationClip> animations;

	D3DXVECTOR3 boundingBoxMin;
	D3DXVECTOR3 boundingBoxMax;

	SceneData()
	{
		rootFrame = nullptr;
		boundingBoxMin = D3DXVECTOR3(FLT_MAX, FLT_MAX, FLT_MAX);
		boundingBoxMax = D3DXVECTOR3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	}

	~SceneData()
	{
		if (rootFrame)
			delete rootFrame;
	}
};

// Utility Functions
namespace Utils
{
	// Convertir string a wstring
	inline wstring StringToWString(const string& str)
	{
		if (str.empty()) return wstring();
		int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
		wstring wstr(size, 0);
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
		return wstr;
	}

	// Convertir wstring a string
	inline string WStringToString(const wstring& wstr)
	{
		if (wstr.empty()) return string();
		int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
		string str(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, NULL, NULL);
		return str;
	}

	// Obtener directorio de un path
	inline string GetDirectory(const string& filepath)
	{
		size_t pos = filepath.find_last_of("\\/");
		if (pos != string::npos)
			return filepath.substr(0, pos + 1);
		return "";
	}

	// Obtener nombre de archivo sin extensión
	inline string GetFilenameWithoutExtension(const string& filepath)
	{
		string filename = filepath;
		size_t pos = filename.find_last_of("\\/");
		if (pos != string::npos)
			filename = filename.substr(pos + 1);

		pos = filename.find_last_of(".");
		if (pos != string::npos)
			filename = filename.substr(0, pos);

		return filename;
	}

    // Verificar si archivo existe
    inline bool FileExists(const string& filepath)
    {
        ifstream file(filepath);
        return file.good();
    }

    // Crear directorio si no existe
    inline bool CreateDirectory(const string& dirpath)
    {
        if (dirpath.empty())
            return false;

        // Crear directorio recursivamente
        string path = dirpath;

        // Normalizar separadores
        for (char& c : path)
        {
            if (c == '/')
                c = '\\';
        }

        // Asegurar que termine con separador
        if (path.back() != '\\')
            path += "\\";

        size_t pos = 0;
        while ((pos = path.find_first_of("\\", pos + 1)) != string::npos)
        {
            string subdir = path.substr(0, pos);
            if (!subdir.empty() && subdir != "." && subdir != "..")
            {
                DWORD attr = ::GetFileAttributesA(subdir.c_str());
                if (attr == INVALID_FILE_ATTRIBUTES)
                {
                    if (!::CreateDirectoryA(subdir.c_str(), NULL))
                    {
                        DWORD error = ::GetLastError();
                        if (error != ERROR_ALREADY_EXISTS)
                            return false;
                    }
                }
                else if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
                {
                    // Existe pero no es un directorio
                    return false;
                }
            }
        }
        return true;
    }

    // Limpiar nombre de archivo (remover caracteres inválidos)
    inline string SanitizeFilename(const string& filename)
    {
        string sanitized = filename;
        const string invalidChars = "<>:\"|?*\\/";
        for (char& c : sanitized)
        {
            if (invalidChars.find(c) != string::npos)
                c = '_';
        }
        return sanitized;
    }

	// Logging
	inline void Log(const string& message, bool verbose = true)
	{
		if (verbose)
			cout << "[INFO] " << message << endl;
	}

	inline void LogWarning(const string& message)
	{
		cout << "[WARNING] " << message << endl;
	}

	inline void LogError(const string& message)
	{
		cerr << "[ERROR] " << message << endl;
	}
}

#endif // COMMON_H
