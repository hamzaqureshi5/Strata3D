cmake_minimum_required(VERSION 3.17)

project(DicomSegmentation)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#======================= INCLUSION OF Qt =======================#
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
if(DEFINED ENV{QTDIR})
    list(APPEND CMAKE_PREFIX_PATH $ENV{QTDIR})
endif()

find_package(OpenGL)
find_package(Qt6 REQUIRED COMPONENTS Core Gui OpenGL OpenGLWidgets Xml Widgets)


#======================= INCLUSION OF DCMTK ======================#
find_package(DCMTK REQUIRED)
include_directories(${DCMTK_INCLUDE_DIRS})

#======================= INCLUSION OF VTK ======================#
if(DEFINED ENV{VTK_DIR})
    set(VTK_DIR $ENV{VTK_DIR})
endif()
find_package(VTK REQUIRED)
if("${VTK_MAJOR_VERSION}" LESS 9)
    include(${VTK_USE_FILE})
endif()

# vtkDICOMReader.h (included by MainWindow.h) comes from vtk-dicom, which is
# either a VTK 9 module or a separate package (Ubuntu: libvtk-dicom-dev).
find_path(VTK_DICOM_INCLUDE_DIR vtkDICOMReader.h
    HINTS ${VTK_PREFIX_PATH}/include
    PATH_SUFFIXES vtk-dicom vtk-${VTK_MAJOR_VERSION}.${VTK_MINOR_VERSION})
if(NOT VTK_DICOM_INCLUDE_DIR)
    message(FATAL_ERROR "vtkDICOMReader.h not found: install libvtk-dicom-dev or build VTK with VTK_MODULE_ENABLE_VTK_DICOM=YES")
endif()
# The header registers a static initializer, so the library must be linked too.
# When VTK itself provides the DICOM module it is already in VTK_LIBRARIES.
if(NOT TARGET VTK::DICOM)
    find_library(VTK_DICOM_LIBRARY
        NAMES vtkDICOM-${VTK_MAJOR_VERSION}.${VTK_MINOR_VERSION} vtkDICOM
        HINTS ${VTK_PREFIX_PATH}/lib)
    if(NOT VTK_DICOM_LIBRARY)
        message(FATAL_ERROR "vtkDICOM library not found: install libvtk-dicom-dev")
    endif()
endif()

#======================= INCLUSION OF Our Code ======================#
set(FORMS_DIR "${CMAKE_SOURCE_DIR}/forms")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(SOURCE_DIR "${CMAKE_SOURCE_DIR}/src")

include_directories(${FORMS_DIR})
include_directories(${INCLUDE_DIR})
include_directories(${SOURCE_DIR})

file(GLOB_RECURSE SOURCES
    "${FORMS_DIR}/*.ui"
    "${INCLUDE_DIR}/*.h"
    "${SOURCE_DIR}/*.cpp"
)

# Add the forms directory to the AUTOUIC search paths
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_AUTOUIC_SEARCH_PATHS} ${FORMS_DIR})

# Add the executable
if (WIN32)
    add_executable(DicomSegmentation WIN32 ${SOURCES})
elseif(UNIX)
    add_executable(DicomSegmentation ${SOURCES})
endif()

# Add the target includes
target_include_directories(DicomSegmentation PRIVATE ${FORMS_DIR})
target_include_directories(DicomSegmentation PRIVATE ${INCLUDE_DIR})
target_include_directories(DicomSegmentation PRIVATE ${SOURCE_DIR})
target_include_directories(DicomSegmentation PRIVATE ${VTK_DICOM_INCLUDE_DIR})

#===================== LINKING LIBRARIES =======================#
target_link_libraries( DicomSegmentation Qt6::OpenGL Qt6::OpenGLWidgets)
target_link_libraries( DicomSegmentation Qt6::Xml)
target_link_libraries( DicomSegmentation Qt6::Widgets)
target_link_libraries( DicomSegmentation ${OPENGL_LIBRARIES})
target_link_libraries( DicomSegmentation ${VTK_LIBRARIES})
target_link_libraries( DicomSegmentation ${DCMTK_LIBRARIES})
if(VTK_DICOM_LIBRARY)
    target_link_libraries( DicomSegmentation ${VTK_DICOM_LIBRARY})
endif()

target_link_libraries( DicomSegmentation dicom2meshlib)

if(NOT "${VTK_MAJOR_VERSION}" LESS 9)
    vtk_module_autoinit(TARGETS DicomSegmentation MODULES ${VTK_LIBRARIES})
endif()

#===================== TOOLS =======================#
# Synthetic dental DICOM series for testing: dental_phantom <out_dir> [--cbct]
add_executable(dental_phantom tools/dental_phantom.cpp)
target_link_libraries(dental_phantom ${DCMTK_LIBRARIES})

add_subdirectory(lib)
add_subdirectory(dicom2mesh)
#add_subdirectory(gui)
