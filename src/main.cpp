//==============================================================================
// X to FBX Converter
// Convierte archivos DirectX .X a formato FBX con soporte completo para:
// - Meshes, Texturas, Materiales
// - Skeleton y Skin Weights
// - Animaciones
// - Conversión de matrices Left-Handed -> Right-Handed
//==============================================================================

#include "../include/Common.h"
#include "XFileParser.h"
#include "FBXExporter.h"
#include "MatrixConverter.h"

// ============================================================================
// Funciones Auxiliares
// ============================================================================

void PrintUsage()
{
    cout << "=============================================================================\n";
    cout << "  X to FBX Converter - DirectX .X to FBX Format Converter\n";
    cout << "=============================================================================\n";
    cout << "\nUSAGE:\n";
    cout << "  XtoFBXConverter.exe <input.x> <output.fbx> [options]\n";
    cout << "\nOPTIONS:\n";
    cout << "  --fbx-version <2020|2019|2018>     FBX version (default: 2020)\n";
    cout << "  --up-axis <Y|Z>                    Up axis (default: Y)\n";
    cout << "  --coordinate-system <RH|LH>        Right/Left handed (default: RH)\n";
    cout << "  --scale <float>                    Global scale factor (default: 1.0)\n";
    cout << "  --export-textures                  Copy textures to output folder\n";
    cout << "  --no-export-textures               Don't copy textures (default)\n";
    cout << "  --triangulate                      Triangulate polygons (default: on)\n";
    cout << "  --fps <30|60>                      Target FPS for animations (default: 30)\n";
    cout << "  --verbose                          Show detailed information\n";
    cout << "  --help                             Show this help message\n";
    cout << "\nEXAMPLES:\n";
    cout << "  # Basic conversion\n";
    cout << "  XtoFBXConverter.exe tiny.x tiny.fbx\n";
    cout << "\n  # For Unity (Right-Handed, Y-Up)\n";
    cout << "  XtoFBXConverter.exe model.x model.fbx --up-axis Y --coordinate-system RH\n";
    cout << "\n  # For Unreal Engine (Z-Up, scaled)\n";
    cout << "  XtoFBXConverter.exe character.x character.fbx --up-axis Z --scale 100.0\n";
    cout << "\n  # With texture export\n";
    cout << "  XtoFBXConverter.exe mesh.x mesh.fbx --export-textures --verbose\n";
    cout << "\n=============================================================================\n";
}

bool ParseArguments(int argc, char* argv[], ConversionOptions& options)
{
    if (argc < 3)
    {
        return false;
    }

    // Archivos de entrada y salida
    options.inputFile = argv[1];
    options.outputFile = argv[2];

    // Parsear opciones
    for (int i = 3; i < argc; i++)
    {
        string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            return false;
        }
        else if (arg == "--fbx-version" && i + 1 < argc)
        {
            string version = argv[++i];
            // FBX SDK 2020.3.7 doesn't use eFBX_* constants anymore
            // We'll use format IDs from the registry instead
            if (version == "2020" || version == "2019" || version == "2018")
                options.fbxVersion = 0; // Will use default format
        }
        else if (arg == "--up-axis" && i + 1 < argc)
        {
            string axis = argv[++i];
            if (axis == "Y" || axis == "y")
                options.upAxis = UpAxis::Y_AXIS;
            else if (axis == "Z" || axis == "z")
                options.upAxis = UpAxis::Z_AXIS;
            else if (axis == "X" || axis == "x")
                options.upAxis = UpAxis::X_AXIS;
        }
        else if (arg == "--coordinate-system" && i + 1 < argc)
        {
            string coordSys = argv[++i];
            if (coordSys == "RH" || coordSys == "rh")
                options.targetCoordSystem = CoordinateSystem::RIGHT_HANDED;
            else if (coordSys == "LH" || coordSys == "lh")
                options.targetCoordSystem = CoordinateSystem::LEFT_HANDED;
        }
        else if (arg == "--scale" && i + 1 < argc)
        {
            options.scale = (float)atof(argv[++i]);
        }
        else if (arg == "--export-textures")
        {
            options.exportTextures = true;
        }
        else if (arg == "--no-export-textures")
        {
            options.exportTextures = false;
        }
        else if (arg == "--triangulate")
        {
            options.triangulate = true;
        }
        else if (arg == "--fps" && i + 1 < argc)
        {
            options.targetFPS = atof(argv[++i]);
            // Validar FPS razonable
            if (options.targetFPS < 1.0 || options.targetFPS > 120.0)
            {
                Utils::LogWarning("Invalid FPS value, using default 30 FPS");
                options.targetFPS = 30.0;
            }
        }
        else if (arg == "--verbose" || arg == "-v")
        {
            options.verbose = true;
        }
        else
        {
            Utils::LogWarning("Unknown argument: " + arg);
        }
    }

    return true;
}

void PrintOptions(const ConversionOptions& options)
{
    cout << "\n--- Conversion Options ---\n";
    cout << "Input file:         " << options.inputFile << "\n";
    cout << "Output file:        " << options.outputFile << "\n";
    cout << "Coordinate system:  " << (options.targetCoordSystem == CoordinateSystem::RIGHT_HANDED ? "Right-Handed" : "Left-Handed") << "\n";
    cout << "Up axis:            " << (options.upAxis == UpAxis::Y_AXIS ? "Y" : options.upAxis == UpAxis::Z_AXIS ? "Z" : "X") << "\n";
    cout << "Global scale:       " << options.scale << "\n";
    cout << "Export textures:    " << (options.exportTextures ? "Yes" : "No") << "\n";
    cout << "Triangulate:        " << (options.triangulate ? "Yes" : "No") << "\n";
    cout << "Verbose:            " << (options.verbose ? "Yes" : "No") << "\n";
    cout << "--------------------------\n\n";
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    cout << "\n";
    cout << "=============================================================================\n";
    cout << "  X to FBX Converter v1.0\n";
    cout << "  DirectX .X to FBX Format Converter\n";
    cout << "=============================================================================\n";
    cout << "\n";

    // Parsear argumentos
    ConversionOptions options;
    if (!ParseArguments(argc, argv, options))
    {
        PrintUsage();
        return 1;
    }

    // Verificar que el archivo de entrada existe
    if (!Utils::FileExists(options.inputFile))
    {
        Utils::LogError("Input file not found: " + options.inputFile);
        return 1;
    }

    // Mostrar opciones
    PrintOptions(options);

    // ========================================================================
    // PASO 1: Cargar archivo .X
    // ========================================================================

    cout << "STEP 1: Loading .X file...\n";

    XFileParser parser;
    SceneData sceneData;

    if (!parser.LoadFile(options.inputFile, sceneData, options))
    {
        Utils::LogError("Failed to load .X file");
        return 1;
    }

    cout << "Successfully loaded .X file!\n";
    cout << "  - Root frame: " << (sceneData.rootFrame ? sceneData.rootFrame->name : "unnamed") << "\n";
    cout << "  - Materials: " << sceneData.materials.size() << "\n";
    cout << "  - Animations: " << sceneData.animations.size() << "\n";
    cout << "\n";

    // ========================================================================
    // PASO 2: Exportar modelo principal a FBX (sin animaciones)
    // ========================================================================

    cout << "STEP 2: Exporting model to FBX...\n";

    FBXExporter exporter;

    // Crear una copia de sceneData sin animaciones para el modelo principal
    SceneData modelData = sceneData;
    modelData.animations.clear(); // No incluir animaciones en el modelo principal

    if (!exporter.ExportScene(modelData, options.outputFile, options))
    {
        Utils::LogError("Failed to export FBX: " + exporter.GetLastError());
        return 1;
    }

    cout << "Successfully exported model to FBX!\n";
    cout << "\n";

    // ========================================================================
    // PASO 3: Exportar animaciones por separado
    // ========================================================================
    // ESTRATEGIA: Exportamos las animaciones en archivos FBX separados
    //
    // ¿Por qué separados?
    //   - Facilita la organización en motores como Unity/Unreal
    //   - Permite cargar solo las animaciones necesarias
    //   - Mejora el rendimiento (no cargar todas las animaciones a la vez)
    //
    // Estructura de archivos resultante:
    //   output.fbx                      ← Modelo principal (geometría + skeleton)
    //   output/
    //     ├─ Walk.fbx                   ← Animación de caminar
    //     ├─ Run.fbx                    ← Animación de correr
    //     └─ Jump.fbx                   ← Animación de saltar
    // ========================================================================

    if (!sceneData.animations.empty())
    {
        cout << "STEP 3: Exporting animations separately...\n";
        cout << "Found " << sceneData.animations.size() << " animation(s)\n\n";

        // ====================================================================
        // Crear directorio para almacenar las animaciones
        // ====================================================================
        // Nombre del directorio será igual al nombre del archivo de entrada
        // Ejemplo: "character.x" → directorio "character/"

        string modelName = Utils::GetFilenameWithoutExtension(options.inputFile);
        string outputDir = Utils::GetDirectory(options.outputFile);
        string animationsDir = outputDir + modelName;

        // Crear el directorio (recursivamente si es necesario)
        if (!Utils::CreateDirectory(animationsDir))
        {
            Utils::LogError("Failed to create animations directory: " + animationsDir);
            return 1;
        }

        cout << "Animations directory: " << animationsDir << "\n\n";

        // ====================================================================
        // Exportar cada animación en un archivo FBX separado
        // ====================================================================
        int exportedCount = 0;
        for (size_t i = 0; i < sceneData.animations.size(); i++)
        {
            const AnimationClip& anim = sceneData.animations[i];

            // Limpiar nombre de animación para usar como nombre de archivo
            string animFilename = Utils::SanitizeFilename(anim.name);
            if (animFilename.empty())
                animFilename = "Animation_" + std::to_string(i + 1);

            string animPath = animationsDir + "\\" + animFilename + ".fbx";

            cout << "Exporting animation " << (i + 1) << "/" << sceneData.animations.size()
                 << ": " << anim.name << " -> " << animPath << "\n";

            if (exporter.ExportSingleAnimation(modelData, anim, animPath, options))
            {
                exportedCount++;
                cout << "  ✓ Successfully exported\n";
            }
            else
            {
                Utils::LogError("  ✗ Failed to export: " + exporter.GetLastError());
            }
        }

        cout << "\n";
        cout << "Exported " << exportedCount << "/" << sceneData.animations.size()
             << " animation(s) successfully\n";
        cout << "Animations saved in: " << animationsDir << "\n\n";
    }
    else
    {
        cout << "STEP 3: No animations found in the file.\n\n";
    }

    // ========================================================================
    // Resumen
    // ========================================================================

    cout << "=============================================================================\n";
    cout << "  CONVERSION COMPLETED SUCCESSFULLY!\n";
    cout << "=============================================================================\n";
    cout << "Output file: " << options.outputFile << "\n";

    if (!sceneData.animations.empty())
    {
        string modelName = Utils::GetFilenameWithoutExtension(options.inputFile);
        string outputDir = Utils::GetDirectory(options.outputFile);
        string animationsDir = outputDir + modelName;
        cout << "Animations exported to: " << animationsDir << "\\\n";
    }

    if (options.exportTextures)
    {
        cout << "Textures exported to: " << Utils::GetDirectory(options.outputFile) << "textures\\\n";
    }

    cout << "\nMatrix Conversion Applied:\n";
    cout << "  DirectX (Left-Handed) -> FBX (Right-Handed)\n";
    cout << "  - Position: (X, Y, Z) -> (X, Y, -Z)\n";
    cout << "  - Normals: (X, Y, Z) -> (X, Y, -Z)\n";
    cout << "  - Winding order: Clockwise -> Counter-Clockwise\n";
    cout << "  - UV coordinates: V inverted\n";
    cout << "\n";

    cout << "You can now import the FBX file into:\n";
    cout << "  - Autodesk Maya\n";
    cout << "  - Autodesk 3ds Max\n";
    cout << "  - Blender\n";
    cout << "  - Unity\n";
    cout << "  - Unreal Engine\n";
    cout << "  - Any other FBX-compatible software\n";
    cout << "\n";
    cout << "=============================================================================\n";

    return 0;
}
