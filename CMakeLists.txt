## Copyright 2023 Stefan Zellmann and Jefferson Amstutz
## SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.1)

project(anariVolumeViewer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

find_package(anari 0.4.0 REQUIRED)
find_package(HDF5 REQUIRED COMPONENTS CXX)

add_executable(${PROJECT_NAME} TransferFunctionEditor.cpp viewer.cpp)
target_link_libraries(${PROJECT_NAME} HDF5::HDF5 anari::anari_viewer)

option(USE_VTK "Support for VTK unstructured grids" OFF)
if (USE_VTK)
  target_sources(${PROJECT_NAME} PRIVATE readVTK.cpp)
  target_compile_definitions(${PROJECT_NAME} PRIVATE -DHAVE_VTK=1)

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
    target_link_libraries(${PROJECT_NAME} ${VTK_LIBRARIES})
  else()
    find_package(VTK COMPONENTS
      CommonColor
      CommonCore
      CommonDataModel
      IOCore
      IOLegacy #ugrid writer
      REQUIRED
    )
    target_link_libraries(${PROJECT_NAME} ${VTK_LIBRARIES})
    vtk_module_autoinit(
      TARGETS ${PROJECT_NAME}
      MODULES ${VTK_LIBRARIES}
    )
  endif()
endif()
