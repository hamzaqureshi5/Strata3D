find_package(Qt6 REQUIRED COMPONENTS Test)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Unit tests of the segmentation core (no display needed)
file(GLOB UNIT_TEST_SOURCES CONFIGURE_DEPENDS unit/*.cpp)
add_executable(strata3d_unit_tests ${UNIT_TEST_SOURCES})
target_link_libraries(strata3d_unit_tests PRIVATE strata3d_core GTest::gtest_main)

# End-to-end test of the main window (needs a desktop session)
add_executable(strata3d_ui_test ui/ui_test.cpp)
target_link_libraries(strata3d_ui_test PRIVATE strata3d_ui Qt6::Test)

set_target_properties(strata3d_unit_tests strata3d_ui_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
vtk_module_autoinit(TARGETS strata3d_unit_tests strata3d_ui_test MODULES ${VTK_LIBRARIES})
