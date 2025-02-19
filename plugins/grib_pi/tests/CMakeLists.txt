cmake_minimum_required(VERSION 3.14)

project(grib_pi_tests)
set(CMAKE_CXX_STANDARD 17)
message(STATUS "Building GRIB plugin tests")

enable_testing()

# Basic dependencies
find_package(Threads REQUIRED)
if (NOT DEFINED wxWidgets_USE_FILE)
    set(wxWidgets_USE_LIBS base core net xml html adv)
    set(BUILD_SHARED_LIBS TRUE)
    find_package(wxWidgets REQUIRED)
endif ()

include(${wxWidgets_USE_FILE})

# Define test source files
set(TEST_SRC
    grib_layer_test.cpp
    grib_layer_set_test.cpp
    mock_defs.cpp
)

# Add the test executable
add_executable(grib_pi_tests ${TEST_SRC})

# Set include directories and defines
target_compile_definitions(grib_pi_tests
    PUBLIC
    TESTDATA="${CMAKE_CURRENT_LIST_DIR}/testdata"
    USE_MOCK_DEFS
    UNIT_TESTS
)

if (NOT UNIX)
  set(SRC_BZIP
      src/bzip2/bzlib.c
      src/bzip2/blocksort.c
      src/bzip2/compress.c
      src/bzip2/crctable.c
      src/bzip2/decompress.c
      src/bzip2/huffman.c
      src/bzip2/randtable.c
  )
  add_library(LIB_BZIP STATIC ${SRC_BZIP})

  include_directories(${CMAKE_SOURCE_DIR}/cache/buildwin/include)
  include_directories(${PLUGIN_SOURCE_DIR}/src/bzip2)
endif (NOT UNIX)

target_include_directories(grib_pi_tests 
    PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/grib_pi/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/model/include
    ${wxWidgets_INCLUDE_DIRS}
)

# Handle OpenGL/GLEW dependencies only if OpenGL is enabled
if(OCPN_USE_GL)
    if(MSVC)
        target_include_directories(grib_pi_tests
            PRIVATE
            ${CMAKE_SOURCE_DIR}/cache/buildwin/include/glew
        )
        target_link_libraries(grib_pi_tests
            PRIVATE
            ${CMAKE_SOURCE_DIR}/cache/buildwin/glew32.lib
        )
    elseif(UNIX AND NOT APPLE)
        find_package(OpenGL REQUIRED)
        find_package(GLEW REQUIRED)
        target_link_libraries(grib_pi_tests PRIVATE ${OPENGL_LIBRARIES} GLEW::GLEW)
    endif()
endif()

# Link libraries
target_link_libraries(grib_pi_tests
    PRIVATE
    gtest
    gtest_main
    Threads::Threads
    ${wxWidgets_LIBRARIES}
    ocpn::model-src
    grib_pi
    win32_libs
)

# Test discovery
include(GoogleTest)
gtest_discover_tests(grib_pi_tests)
