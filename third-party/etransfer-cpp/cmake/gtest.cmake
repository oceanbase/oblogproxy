include(ExternalProject)

set(GTEST_EXTERNAL_ROOT ${THIRD_PARTY_PATH}/gtest)

set(GTEST_SOURCE_DIR ${GTEST_EXTERNAL_ROOT}/src/extern_gtest)

set(GTEST_INSTALL_DIR ${THIRD_PARTY_PATH}/install/gtest)

ExternalProject_Add(
        extern_gtest
        BUILD_BYPRODUCTS ${GTEST_INSTALL_DIR}/lib/libgtest.a
        PREFIX ${GTEST_EXTERNAL_ROOT}
        SOURCE_DIR ${GTEST_SOURCE_DIR}
        INSTALL_DIR ${GTEST_INSTALL_DIR}
        GIT_REPOSITORY "https://github.com/google/googletest.git"
        GIT_TAG "release-1.11.0"
        CMAKE_ARGS
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
        -DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}
        -DCMAKE_C_FLAGS:STRING=${CMAKE_C_FLAGS}

        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        -DCMAKE_INSTALL_LIBDIR:PATH=<INSTALL_DIR>/lib
        -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON

        -DBUILD_GMOCK:BOOL=OFF
        -Dgtest_build_samples:BOOL=OFF
        -Dgtest_build_tests:BOOL=OFF
        -Dgtest_disable_pthreads:BOOL=ON
)

if (NOT EXISTS ${GTEST_INSTALL_DIR}/include)
    execute_process(COMMAND mkdir -p ${GTEST_INSTALL_DIR}/include COMMAND_ERROR_IS_FATAL ANY)
endif ()

add_library(gtest STATIC IMPORTED GLOBAL)
add_dependencies(gtest extern_gtest)
set_target_properties(gtest PROPERTIES IMPORTED_LOCATION ${GTEST_INSTALL_DIR}/lib/libgtest.a)
target_include_directories(gtest INTERFACE ${GTEST_INSTALL_DIR}/include)