cmake_minimum_required(VERSION 3.17)

project(DicomSegmentation)

#======================= INCLUSION OF Qt =======================#
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_PREFIX_PATH $ENV{QTDIR})


find_package(OpenGL)
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5OpenGL REQUIRED)
find_package(Qt5Xml REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(GDCM REQUIRED)

#======================= INCLUSION OF GDCM ======================#

#include_directories("C:/Program Files/GDCM 3.0/include")
#link_directories("C:/Program Files/GDCM 3.0/lib")
# Get the GDCM_DIR environment variable
#set(GDCM_DIR $ENV{GDCM_DIR})
#set(GDCM_INCLUDE_DIR "C:/Program Files/GDCM/include")
#set(GDCM_LIBRARY_DIR "C:/Program Files/GDCM/lib")

# Include directories and library directories
#include_directories(${GDCM_INCLUDE_DIR})
#link_directories(${GDCM_LIBRARY_DIR})


# Debug messages to verify paths
#message("GDCM Root Directory: ${GDCM_DIR}")
#message("GDCM Include Directory: ${GDCM_INCLUDE_DIR}")
#message("GDCM Library Directory: ${GDCM_LIBRARY_DIR}")


find_package(GDCM REQUIRED)
include_directories(${GDCM_INCLUDE_DIRS})
link_directories(${GDCM_LIBRARY_DIRS})

#======================= INCLUSION OF VTK ======================#
set(VTK_DIR $ENV{VTK_DIR})
find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

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

# Add resources to the executable
if (WIN32) 
    add_executable(DicomSegmentation WIN32 ${SOURCES})
elseif(UNIX)
    add_executable(DicomSegmentation ${SOURCES})
endif()


# Add the target includes
target_include_directories(DicomSegmentation PRIVATE ${FORMS_DIR})
target_include_directories(DicomSegmentation PRIVATE ${INCLUDE_DIR})
target_include_directories(DicomSegmentation PRIVATE ${SOURCE_DIR})

#===================== LINKING LIBRARIES =======================#
target_link_libraries( DicomSegmentation Qt5::OpenGL)
target_link_libraries( DicomSegmentation Qt5::Xml)
target_link_libraries( DicomSegmentation Qt5::Widgets)
target_link_libraries( DicomSegmentation ${QT_LIBRARIES} ${OPENGL_LIBRARIES})
target_link_libraries( DicomSegmentation ${VTK_LIBRARIES})
target_link_libraries( DicomSegmentation ${GDCM_LIBRARIES})
target_link_libraries( DicomSegmentation dicom2meshlib)

add_subdirectory(lib)
# add_subdirectory(dicom2mesh)
# add_subdirectory(gui)