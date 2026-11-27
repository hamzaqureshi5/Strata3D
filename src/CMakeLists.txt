# Layers (each depends only on the ones above it):
#   strata3d_core  segmentation domain: VTK + dicom2meshlib, no Qt
#   strata3d_ui    Qt widgets, VTK views, worker threads
#   Strata3D       the executable: logging setup and main()

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS core/*.cpp core/*.h)
add_library(strata3d_core STATIC ${CORE_SOURCES})
target_include_directories(strata3d_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(strata3d_core PUBLIC ${VTK_LIBRARIES} dicom2meshlib)

file(GLOB UI_SOURCES CONFIGURE_DEPENDS ui/*.cpp ui/*.h ui/*.ui)
add_library(strata3d_ui STATIC ${UI_SOURCES})
set_target_properties(strata3d_ui PROPERTIES AUTOMOC ON AUTOUIC ON)
# generated ui_main_window.h, for the UI test
target_include_directories(strata3d_ui PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}/strata3d_ui_autogen/include)
target_link_libraries(strata3d_ui PUBLIC strata3d_core
    Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent)

add_executable(Strata3D WIN32 app/main.cpp app/logging.cpp app/logging.h)
target_link_libraries(Strata3D PRIVATE strata3d_ui)
# keep the binary at build/Strata3D
set_target_properties(Strata3D PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# VTK 9 object factories (rendering backends) are registered per target
vtk_module_autoinit(TARGETS strata3d_core strata3d_ui Strata3D MODULES ${VTK_LIBRARIES})
