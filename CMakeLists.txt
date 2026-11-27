cmake_minimum_required(VERSION 3.24) # 3rdparty/DicomToMesh needs 3.24

project(Strata3D LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#============================ DEPENDENCIES ============================#
# Optional hints for non-standard installations
if(DEFINED ENV{QTDIR})
    list(APPEND CMAKE_PREFIX_PATH $ENV{QTDIR})
endif()
if(DEFINED ENV{VTK_DIR})
    set(VTK_DIR $ENV{VTK_DIR})
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL OpenGLWidgets Widgets Concurrent)
find_package(VTK REQUIRED)
# Only the synthetic test-data generator (tools/dental_phantom) needs DCMTK
find_package(DCMTK QUIET)

option(STRATA3D_BUILD_TESTS "Build the unit tests and the end-to-end UI test" OFF)
option(STRATA3D_BUILD_DICOM2MESH_CLI "Build the upstream dicom2mesh command-line tool" OFF)

#============================== TARGETS ===============================#
# Third-party code as git submodules (git submodule update --init)
set(DICOMTOMESH_DIR ${CMAKE_SOURCE_DIR}/3rdparty/DicomToMesh)
if(NOT EXISTS ${DICOMTOMESH_DIR}/lib/CMakeLists.txt)
    message(FATAL_ERROR "3rdparty/DicomToMesh is missing: run "
                        "git submodule update --init --recursive")
endif()
# dicom2meshlib: VTK routines for DICOM loading, marching cubes and mesh cleanup
add_subdirectory(${DICOMTOMESH_DIR}/lib ${CMAKE_BINARY_DIR}/3rdparty/dicom2meshlib)
if(STRATA3D_BUILD_DICOM2MESH_CLI)
    add_subdirectory(${DICOMTOMESH_DIR}/dicom2mesh ${CMAKE_BINARY_DIR}/3rdparty/dicom2mesh)
endif()

add_subdirectory(src) # strata3d_core, strata3d_ui, Strata3D

if(STRATA3D_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Synthetic dental DICOM series for testing: dental_phantom <out_dir> [--cbct]
if(DCMTK_FOUND)
    add_executable(dental_phantom tools/dental_phantom.cpp)
    target_include_directories(dental_phantom PRIVATE ${DCMTK_INCLUDE_DIRS})
    target_link_libraries(dental_phantom PRIVATE ${DCMTK_LIBRARIES})
else()
    message(STATUS "DCMTK not found: tools/dental_phantom is not built")
endif()
