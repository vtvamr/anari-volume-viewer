## Copyright 2023 Stefan Zellmann and Jefferson Amstutz
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.10)

project(anariVolumeViewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

set(glm_DIR ${CMAKE_CURRENT_LIST_DIR}/external/glm/lib/cmake/glm)
find_package(glm REQUIRED)
mark_as_advanced(glm_DIR)

find_package(anari 0.8.0 REQUIRED COMPONENTS viewer)

add_executable(${PROJECT_NAME} TransferFunctionEditor.cpp viewer.cpp)
target_link_libraries(${PROJECT_NAME} glm::glm anari::anari_viewer)

option(USE_HDF5 "Support loading AMR grids from HDF5" OFF)
if (USE_HDF5)
  find_package(HDF5 REQUIRED COMPONENTS CXX)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DHAVE_HDF5)
  target_link_libraries(${PROJECT_NAME} HDF5::HDF5)
endif()

option(USE_UMESH "Support for umesh unstructured grids" OFF)
if (USE_UMESH)
  #
  # first try find_package in CONFIG mode, it will be catched by OneTBB
  # if OneTBB is not avaible, then use the old macro
  #
  find_package(TBB CONFIG)
  if (NOT TBB_FOUND)
    find_package(TBB)
  endif()
  find_package(umesh)
  if (umesh_FOUND)
    target_sources(${PROJECT_NAME} PRIVATE readUMesh.cpp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DHAVE_UMESH)
    target_link_libraries(${PROJECT_NAME} umesh::umesh)
  endif()
endif()

option(USE_VTK "Support for VTK unstructured grids" OFF)
if (USE_VTK)
  target_sources(${PROJECT_NAME} PRIVATE readVTK.cpp)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DHAVE_VTK)

  if (VTK_VERSION VERSION_LESS "8.90.0")
    find_package(VTK COMPONENTS
      vtkCommonColor
      vtkCommonCore
      vtkCommonDataModel
      vtkIOCore
      vtkIOLegacy #ugrid writer
      REQUIRED
    )
    include(${VTK_USE_FILE})
  else()
    find_package(VTK COMPONENTS
      CommonColor
      CommonCore
      CommonDataModel
      IOCore
      IOLegacy #ugrid writer
      REQUIRED
    )
    vtk_module_autoinit(
      TARGETS ${PROJECT_NAME}
      MODULES ${VTK_LIBRARIES}
    )
  endif()

  target_link_libraries(${PROJECT_NAME} ${VTK_LIBRARIES})
endif()
