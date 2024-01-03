// Copyright 2023 Stefan Zellmann and Jefferson Amstutz
// SPDX-License-Identifier: Apache-2.0

// anari_viewer
#include "anari_viewer/Application.h"
#include "anari_viewer/windows/LightsEditor.h"
#include "anari_viewer/windows/Viewport.h"
// glm
#include "glm/gtc/matrix_transform.hpp"
// std
#include <algorithm>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
// ours
#include "FieldTypes.h"
#include "ISOSurfaceEditor.h"
#include "TransferFunctionEditor.h"
#include "readRAW.h"
#ifdef HAVE_HDF5
#include "readFlash.h"
#endif
#ifdef HAVE_VTK
#include "readVTK.h"
#endif
#ifdef HAVE_UMESH
#include "readUMesh.h"
#endif

static const bool g_true = true;
static bool g_verbose = false;
static bool g_useDefaultLayout = true;
static bool g_enableDebug = false;
static std::string g_libraryName = "environment";
static anari::Library g_debug = nullptr;
static anari::Device g_device = nullptr;
static const char *g_traceDir = nullptr;
static std::string g_filename;
static int g_dimX = 0, g_dimY = 0, g_dimZ = 0;
static unsigned g_bytesPerCell = 0;
static float g_voxelRange[2];

static const char *g_defaultLayout =
    R"layout(
[Window][MainDockSpace]
Pos=0,25
Size=1440,813
Collapsed=0

[Window][Viewport]
Pos=551,25
Size=889,813
Collapsed=0
DockId=0x00000003,0

[Window][Lights Editor]
Pos=0,25
Size=549,813
Collapsed=0
DockId=0x00000002,1

[Window][TF Editor]
Pos=0,25
Size=549,813
Collapsed=0
DockId=0x00000002,0

[Window][Debug##Default]
Pos=60,60
Size=400,400
Collapsed=0

[Window][ISO Editor]
Pos=0,557
Size=549,438
Collapsed=0
DockId=0x00000004,0

[Docking][Data]
DockSpace   ID=0x782A6D6B Window=0xDEDC5B90 Pos=0,25 Size=1440,813 Split=X
  DockNode  ID=0x00000002 Parent=0x782A6D6B SizeRef=549,1174 Selected=0xE3280322
  DockNode  ID=0x00000003 Parent=0x782A6D6B SizeRef=1369,1174 CentralNode=1 Selected=0x13926F0B
)layout";

namespace viewer {

struct AppState
{
  manipulators::Orbit manipulator;
  anari::Device device{nullptr};
  anari::World world{nullptr};
  anari::SpatialField field{nullptr};
  AMRField data;
  UnstructuredField udata;
  StructuredField sdata;
#ifdef HAVE_HDF5
  FlashReader flashReader;
#endif
#ifdef HAVE_VTK
  VTKReader vtkReader;
#endif
#ifdef HAVE_UMESH
  UMeshReader umeshReader;
#endif
  RAWReader rawReader;
};

static void statusFunc(const void *userData,
    ANARIDevice device,
    ANARIObject source,
    ANARIDataType sourceType,
    ANARIStatusSeverity severity,
    ANARIStatusCode code,
    const char *message)
{
  const bool verbose = userData ? *(const bool *)userData : false;
  if (severity == ANARI_SEVERITY_FATAL_ERROR) {
    fprintf(stderr, "[FATAL][%p] %s\n", source, message);
    std::exit(1);
  } else if (severity == ANARI_SEVERITY_ERROR)
    fprintf(stderr, "[ERROR][%p] %s\n", source, message);
  else if (severity == ANARI_SEVERITY_WARNING)
    fprintf(stderr, "[WARN ][%p] %s\n", source, message);
  else if (verbose && severity == ANARI_SEVERITY_PERFORMANCE_WARNING)
    fprintf(stderr, "[PERF ][%p] %s\n", source, message);
  else if (verbose && severity == ANARI_SEVERITY_INFO)
    fprintf(stderr, "[INFO ][%p] %s\n", source, message);
  else if (verbose && severity == ANARI_SEVERITY_DEBUG)
    fprintf(stderr, "[DEBUG][%p] %s\n", source, message);
}

static std::string getExt(const std::string &fileName)
{
  int pos = fileName.rfind('.');
  if (pos == fileName.npos)
    return "";
  return fileName.substr(pos);
}

static std::vector<std::string> string_split(std::string s, char delim)
{
  std::vector<std::string> result;

  std::istringstream stream(s);

  for (std::string token; std::getline(stream, token, delim);) {
    result.push_back(token);
  }

  return result;
}

static void initializeANARI()
{
  auto library =
      anariLoadLibrary(g_libraryName.c_str(), statusFunc, &g_verbose);
  if (!library)
    throw std::runtime_error("Failed to load ANARI library");

  if (g_enableDebug)
    g_debug = anariLoadLibrary("debug", statusFunc, &g_true);

  anari::Device dev = anariNewDevice(library, "default");

  anari::unloadLibrary(library);

  if (g_enableDebug)
    anari::setParameter(dev, dev, "glDebug", true);

#ifdef USE_GLES2
  anari::setParameter(dev, dev, "glAPI", "OpenGL_ES");
#else
  anari::setParameter(dev, dev, "glAPI", "OpenGL");
#endif

  if (g_enableDebug) {
    anari::Device dbg = anariNewDevice(g_debug, "debug");
    anari::setParameter(dbg, dbg, "wrappedDevice", dev);
    if (g_traceDir) {
      anari::setParameter(dbg, dbg, "traceDir", g_traceDir);
      anari::setParameter(dbg, dbg, "traceMode", "code");
    }
    anari::commitParameters(dbg, dbg);
    anari::release(dev, dev);
    dev = dbg;
  }

  anari::commitParameters(dev, dev);

  g_device = dev;
}

// Application definition /////////////////////////////////////////////////////

class Application : public anari_viewer::Application
{
 public:
  Application() = default;
  ~Application() override = default;

  anari_viewer::WindowArray setup() override
  {
    ui::init();

    // If file type is raw, try to guess dimensions and data type
    // (if not already set)
    if (getExt(g_filename) == ".raw" && !g_dimX && !g_dimY && !g_dimZ
        && !g_bytesPerCell) {
      std::vector<std::string> strings;
      strings = string_split(g_filename, '_');

      for (auto str : strings) {
        int dimx, dimy, dimz;
        int res = sscanf(str.c_str(), "%ix%ix%i", &dimx, &dimy, &dimz);
        if (res == 3) {
          g_dimX = dimx;
          g_dimY = dimy;
          g_dimZ = dimz;
        }

        int bits = 0;
        res = sscanf(str.c_str(), "int%i", &bits);
        if (res == 1)
          g_bytesPerCell = bits / 8;

        res = sscanf(str.c_str(), "uint%i", &bits);
        if (res == 1)
          g_bytesPerCell = bits / 8;

        if (g_dimX && g_dimY && g_dimZ && g_bytesPerCell)
          break;
      }

      if (!g_bytesPerCell)
        g_bytesPerCell = 4;

      if (g_dimX && g_dimY && g_dimZ && g_bytesPerCell) {
        std::cout
            << "Guessing dimensions and data type from file name: [dims x/y/z]: "
            << g_dimX << " x " << g_dimY << " x " << g_dimZ << ", "
            << g_bytesPerCell << " byte(s)/cell\n";
      }
    }

    // ANARI //

    initializeANARI();

    auto device = g_device;

    if (!device)
      std::exit(1);

    m_state.device = device;
    m_state.world = anari::newObject<anari::World>(device);

    // Setup scene //

    if (g_dimX && g_dimY && g_dimZ && g_bytesPerCell
        && m_state.rawReader.open(
            g_filename.c_str(), g_dimX, g_dimY, g_dimZ, g_bytesPerCell)) {
      m_state.sdata = m_state.rawReader.getField(0);
      auto &data = m_state.sdata;

      auto field =
          anari::newObject<anari::SpatialField>(device, "structuredRegular");

      anari::Array3D scalar;
      if (data.bytesPerCell == 1) {
        scalar = anariNewArray3D(device,
            data.dataUI8.data(),
            0,
            0,
            ANARI_UFIXED8,
            g_dimX,
            g_dimY,
            g_dimZ);
      } else if (data.bytesPerCell == 2) {
        scalar = anariNewArray3D(device,
            data.dataUI16.data(),
            0,
            0,
            ANARI_UFIXED16,
            g_dimX,
            g_dimY,
            g_dimZ);
      } else if (data.bytesPerCell == 4) {
        scalar = anariNewArray3D(device,
            data.dataF32.data(),
            0,
            0,
            ANARI_FLOAT32,
            g_dimX,
            g_dimY,
            g_dimZ);
      }

      anari::setAndReleaseParameter(device, field, "data", scalar);
      anari::setParameter(device, field, "filter", ANARI_STRING, "linear");

      anari::commitParameters(device, field);
      m_state.field = field;

      g_voxelRange[0] = data.dataRange.x;
      g_voxelRange[1] = data.dataRange.y;
    }
#ifdef HAVE_HDF5
    else if (m_state.flashReader.open(g_filename.c_str())) {
      m_state.data = m_state.flashReader.getField(0);
      auto &data = m_state.data;

      auto field = anari::newObject<anari::SpatialField>(device, "amr");

      std::vector<anari::Array3D> blockDataV(data.blockData.size());
      for (size_t i = 0; i < data.blockData.size(); ++i) {
        blockDataV[i] = anari::newArray3D(device,
            data.blockData[i].values.data(),
            data.blockData[i].dims[0],
            data.blockData[i].dims[1],
            data.blockData[i].dims[2]);
      }

      printf("Array sizes:\n");
      printf("    'cellWidth'  : %zu\n", data.cellWidth.size());
      printf("    'blockBounds': %zu\n", data.blockBounds.size());
      printf("    'blockLevel' : %zu\n", data.blockLevel.size());
      printf("    'blockData'  : %zu\n", blockDataV.size());

      anari::setParameterArray1D(device,
          field,
          "cellWidth",
          ANARI_FLOAT32,
          data.cellWidth.data(),
          data.cellWidth.size());
      anari::setParameterArray1D(device,
          field,
          "block.bounds",
          ANARI_INT32_BOX3,
          data.blockBounds.data(),
          data.blockBounds.size());
      anari::setParameterArray1D(device,
          field,
          "block.level",
          ANARI_INT32,
          data.blockLevel.data(),
          data.blockLevel.size());
      anari::setParameterArray1D(device,
          field,
          "block.data",
          ANARI_ARRAY1D,
          blockDataV.data(),
          blockDataV.size());

      for (auto a : blockDataV)
        anari::release(device, a);

      anari::commitParameters(device, field);
      m_state.field = field;

      g_voxelRange[0] = data.voxelRange.x;
      g_voxelRange[1] = data.voxelRange.y;
    }
#endif
#ifdef HAVE_VTK
    else if (m_state.vtkReader.open(g_filename.c_str())) {
      bool indexPrefixed = false;
      m_state.udata = m_state.vtkReader.getField(0, indexPrefixed);
      auto &data = m_state.udata;

      auto field =
          anari::newObject<anari::SpatialField>(device, "unstructured");

      printf("Array sizes:\n");
      printf("    'vertexPosition': %zu\n", data.vertexPosition.size());
      printf("    'vertexData'    : %zu\n", data.vertexData.size());
      printf("    'index'         : %zu\n", data.index.size());
      printf("    'cellIndex'     : %zu\n", data.cellIndex.size());
      printf("    'cellType'      : %zu\n", data.cellType.size());

      anari::setParameterArray1D(device,
          field,
          "vertex.position",
          ANARI_FLOAT32_VEC3,
          data.vertexPosition.data(),
          data.vertexPosition.size());
      anari::setParameterArray1D(device,
          field,
          "vertex.data",
          ANARI_FLOAT32,
          data.vertexData.data(),
          data.vertexData.size());
      anari::setParameterArray1D(device,
          field,
          "index",
          ANARI_UINT64,
          data.index.data(),
          data.index.size());
      anari::setParameter(
          device, field, "indexPrefixed", ANARI_BOOL, &data.indexPrefixed);
      anari::setParameterArray1D(device,
          field,
          "cell.index",
          ANARI_UINT64,
          data.cellIndex.data(),
          data.cellIndex.size());
      anari::setParameterArray1D(device,
          field,
          "cell.type",
          ANARI_UINT8,
          data.cellType.data(),
          data.cellType.size());

      anari::commitParameters(device, field);
      m_state.field = field;

      g_voxelRange[0] = data.dataRange.x;
      g_voxelRange[1] = data.dataRange.y;
    }
#endif
#ifdef HAVE_UMESH
    else if (m_state.umeshReader.open(g_filename.c_str())) {
      m_state.udata = m_state.umeshReader.getField(0);
      auto &data = m_state.udata;

      auto field =
          anari::newObject<anari::SpatialField>(device, "unstructured");

      printf("Array sizes:\n");
      printf("    'vertexPosition': %zu\n", data.vertexPosition.size());
      printf("    'vertexData'    : %zu\n", data.vertexData.size());
      printf("    'index'         : %zu\n", data.index.size());
      printf("    'cellIndex'     : %zu\n", data.cellIndex.size());
      printf("    'cellType'      : %zu\n", data.cellType.size());
      printf("    'gridData'      : %zu\n", data.gridData.size());
      printf("    'gridDomains'   : %zu\n", data.gridDomains.size());

      anari::setParameterArray1D(device,
          field,
          "vertex.position",
          ANARI_FLOAT32_VEC3,
          data.vertexPosition.data(),
          data.vertexPosition.size());
      anari::setParameterArray1D(device,
          field,
          "vertex.data",
          ANARI_FLOAT32,
          data.vertexData.data(),
          data.vertexData.size());
      anari::setParameterArray1D(device,
          field,
          "index",
          ANARI_UINT64,
          data.index.data(),
          data.index.size());
      anari::setParameter(
          device, field, "indexPrefixed", ANARI_BOOL, &data.indexPrefixed);
      anari::setParameterArray1D(device,
          field,
          "cell.index",
          ANARI_UINT64,
          data.cellIndex.data(),
          data.cellIndex.size());
      anari::setParameterArray1D(device,
          field,
          "cell.type",
          ANARI_UINT8,
          data.cellType.data(),
          data.cellType.size());

      if (!data.gridData.empty() && !data.gridDomains.empty()) {
        std::vector<anari::Array3D> gridDataV(data.gridData.size());
        for (size_t i = 0; i < data.gridData.size(); ++i) {
          gridDataV[i] = anari::newArray3D(device,
              data.gridData[i].values.data(),
              data.gridData[i].dims[0],
              data.gridData[i].dims[1],
              data.gridData[i].dims[2]);
        }

        anari::setParameterArray1D(device,
            field,
            "grid.data",
            ANARI_ARRAY1D,
            gridDataV.data(),
            gridDataV.size());
        anari::setParameterArray1D(device,
            field,
            "grid.domains",
            ANARI_FLOAT32_BOX3,
            data.gridDomains.data(),
            data.gridDomains.size());
      }

      anari::commitParameters(device, field);
      m_state.field = field;

      g_voxelRange[0] = data.dataRange.x;
      g_voxelRange[1] = data.dataRange.y;
    }
#endif

    // Volume //

    auto volume = anari::newObject<anari::Volume>(device, "transferFunction1D");
    anari::setParameter(device, volume, "field", m_state.field);

    {
      std::vector<anari::math::float3> colors;
      std::vector<float> opacities;

      colors.emplace_back(0.f, 0.f, 1.f);
      colors.emplace_back(0.f, 1.f, 0.f);
      colors.emplace_back(1.f, 0.f, 0.f);

      opacities.emplace_back(0.f);
      opacities.emplace_back(1.f);

      anari::setAndReleaseParameter(device,
          volume,
          "color",
          anari::newArray1D(device, colors.data(), colors.size()));
      anari::setAndReleaseParameter(device,
          volume,
          "opacity",
          anari::newArray1D(device, opacities.data(), opacities.size()));
      anariSetParameter(
          device, volume, "valueRange", ANARI_FLOAT32_BOX1, &g_voxelRange);
    }

    anari::commitParameters(device, volume);

#if 0
    anari::setAndReleaseParameter(
        device, m_state.world, "volume", anari::newArray1D(device, &volume));
    anari::release(device, volume);
#endif

    // ISO Surface geom //

    auto geometry = anari::newObject<anari::Geometry>(device, "isosurface");
    anari::setParameter(device, geometry, "field", m_state.field);

    anari::commitParameters(device, geometry);

    // Create color map texture //

    auto texelArray = anari::newArray1D(device, ANARI_FLOAT32_VEC3, 2);
    {
      auto *texels = anari::map<glm::vec3>(device, texelArray);
      texels[0][0] = 1.f;
      texels[0][1] = 0.f;
      texels[0][2] = 0.f;
      texels[1][0] = 0.f;
      texels[1][1] = 1.f;
      texels[1][2] = 0.f;
      anari::unmap(device, texelArray);
    }

    // Map iso values from raw to [0,1]:
    glm::vec4 inOffset(
        -g_voxelRange[0] / (g_voxelRange[1] - g_voxelRange[0]), 0, 0, 0);
    glm::mat4 inTransform = glm::scale(glm::mat4(1.0f),
        glm::vec3(1.f / (g_voxelRange[1] - g_voxelRange[0]), 1.f, 1.f));

    auto texture = anari::newObject<anari::Sampler>(device, "image1D");
    anari::setAndReleaseParameter(device, texture, "image", texelArray);
    anari::setParameter(device, texture, "inAttribute", "attribute0");
    anari::setParameter(device, texture, "filter", "linear");
    anari::setParameter(device, texture, "inOffset", inOffset);
    anari::setParameter(device, texture, "inTransform", inTransform);
    anari::commitParameters(device, texture);

    // Create and parameterize material //

    auto material = anari::newObject<anari::Material>(device, "matte");
    anari::setAndReleaseParameter(device, material, "color", texture);
    anari::commitParameters(device, material);

    // Create and parameterize surface //

    auto surface = anari::newObject<anari::Surface>(device);
    anari::setAndReleaseParameter(device, surface, "geometry", geometry);
    anari::setAndReleaseParameter(device, surface, "material", material);
    anari::commitParameters(device, surface);

    anari::setAndReleaseParameter(
        device, m_state.world, "surface", anari::newArray1D(device, &surface));

    anari::commitParameters(device, m_state.world);

    // ImGui //

    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = 1.5f;
    io.IniFilename = nullptr;

    if (g_useDefaultLayout)
      ImGui::LoadIniSettingsFromMemory(g_defaultLayout);

    auto *viewport = new windows::Viewport(device, "Viewport");
    viewport->setManipulator(&m_state.manipulator);
    viewport->setWorld(m_state.world);
    viewport->resetView();

    auto *leditor = new windows::LightsEditor({device});
    leditor->setWorlds({m_state.world});

    auto *tfeditor = new windows::TransferFunctionEditor();
    tfeditor->setValueRange({g_voxelRange[0], g_voxelRange[1]});
    tfeditor->setUpdateCallback(
        [=](const glm::vec2 &valueRange, const std::vector<glm::vec4> &co) {
          std::vector<glm::vec3> colors(co.size());
          std::vector<float> opacities(co.size());
          std::transform(
              co.begin(), co.end(), colors.begin(), [](const glm::vec4 &v) {
                return glm::vec3(v);
              });
          std::transform(
              co.begin(), co.end(), opacities.begin(), [](const glm::vec4 &v) {
                return v.w;
              });
          anari::setParameterArray1D(device,
              volume,
              "color",
              ANARI_FLOAT32_VEC3,
              colors.data(),
              colors.size());
          anari::setParameterArray1D(device,
              volume,
              "opacity",
              ANARI_FLOAT32,
              opacities.data(),
              opacities.size());
          anariSetParameter(
              device, volume, "valueRange", ANARI_FLOAT32_BOX1, &valueRange);

          anari::commitParameters(device, volume);
          auto texelArray = anari::newArray1D(device, ANARI_FLOAT32_VEC3, colors.size());
          {
            auto *texels = anari::map<glm::vec3>(device, texelArray);
            for (int i=0; i<colors.size(); i++) {
              texels[i] = colors[i];
            }
            anari::unmap(device, texelArray);
          }
          anari::setAndReleaseParameter(device, texture, "image", texelArray);
          anari::commitParameters(device, texture);
        });


    // ISO values
    auto *isoeditor = new windows::ISOSurfaceEditor();
    isoeditor->setValueRange({g_voxelRange[0], g_voxelRange[1]});
    isoeditor->setUpdateCallback(
        [=](const std::vector<float> &isoValues) {
      anari::setAndReleaseParameter(device,
          geometry,
          "isovalue",
          anari::newArray1D(device, isoValues.data(), isoValues.size()));

      anari::setAndReleaseParameter(device,
          geometry,
          "primitive.attribute0",
          anari::newArray1D(device, isoValues.data(), isoValues.size()));
      anari::commitParameters(device, geometry);
    });

    anari_viewer::WindowArray windows;
    windows.emplace_back(viewport);
    windows.emplace_back(leditor);
    windows.emplace_back(tfeditor);
    windows.emplace_back(isoeditor);

    return windows;
  }

  void buildMainMenuUI() override
  {
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("print ImGui ini")) {
          const char *info = ImGui::SaveIniSettingsToMemory();
          printf("%s\n", info);
        }

        ImGui::EndMenu();
      }

#ifdef HAVE_HDF5
      if (ImGui::BeginMenu("Volume")) {
        ImGui::Text("METHOD:");
        auto d = m_state.device;
        auto f = m_state.field;
        static int e = 0;
        int old_e = e;
        if (ImGui::RadioButton("current", &e, 0))
          anari::setParameter(d, f, "method", "current");
        if (ImGui::RadioButton("finest", &e, 1))
          anari::setParameter(d, f, "method", "finest");
        if (ImGui::RadioButton("octant", &e, 2))
          anari::setParameter(d, f, "method", "octant");

        if (old_e != e)
          anari::commitParameters(d, f);

        ImGui::EndMenu();
      }
#endif

      ImGui::EndMainMenuBar();
    }
  }

  void teardown() override
  {
    anari::release(m_state.device, m_state.field);
    anari::release(m_state.device, m_state.world);
    anari::release(m_state.device, m_state.device);
    ui::shutdown();
  }

 private:
  AppState m_state;
};

} // namespace viewer

///////////////////////////////////////////////////////////////////////////////

static void printUsage()
{
  std::cout << "./anariVolumeViewer [{--help|-h}]\n"
            << "   [{--verbose|-v}] [{--debug|-g}]\n"
            << "   [{--library|-l} <ANARI library>]\n"
            << "   [{--trace|-t} <directory>]\n"
            << "   [{--dims|-d} <dimx dimy dimz>]\n"
            << "   [{--type|-t} [{uint8|uint16|float32}]\n";
}

static void parseCommandLine(int argc, char *argv[])
{
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "-v" || arg == "--verbose")
      g_verbose = true;
    if (arg == "--help" || arg == "-h") {
      printUsage();
      std::exit(0);
    } else if (arg == "--noDefaultLayout")
      g_useDefaultLayout = false;
    else if (arg == "-l" || arg == "--library")
      g_libraryName = argv[++i];
    else if (arg == "--debug" || arg == "-g")
      g_enableDebug = true;
    else if (arg == "--trace")
      g_traceDir = argv[++i];
    else if (arg == "--dims" || arg == "-d") {
      g_dimX = std::atoi(argv[++i]);
      g_dimY = std::atoi(argv[++i]);
      g_dimZ = std::atoi(argv[++i]);
    } else if (arg == "--type" || arg == "-t") {
      std::string v = argv[++i];
      if (v == "uint8")
        g_bytesPerCell = 1;
      else if (v == "uint16")
        g_bytesPerCell = 2;
      else if (v == "float32")
        g_bytesPerCell = 4;
      else {
        printUsage();
        std::exit(0);
      }
    } else
      g_filename = std::move(arg);
  }
}

int main(int argc, char *argv[])
{
  parseCommandLine(argc, argv);
  if (g_filename.empty()) {
    printf("ERROR: no input file provided\n");
    std::exit(1);
  }
  viewer::Application app;
  app.run(1920, 1200, "ANARI Volume Viewer");
  return 0;
}
