include(ExternalProject)

set(GTEST_SOURCES_DIR ${THIRD_PARTY_PATH}/gtest)
set(GTEST_INSTALL_DIR ${THIRD_PARTY_PATH}/install/gtest)
set(GTEST_INCLUDE_DIR "${GTEST_INSTALL_DIR}/include" CACHE PATH "gtest include directory." FORCE)
set(GTEST_LIBRARIES "${GTEST_INSTALL_DIR}/lib64/libgtest.a" CACHE FILEPATH "gtest library." FORCE)
set(INSTALL_COMMAND $(MAKE) install)

include_directories(${GTEST_INCLUDE_DIR})

set(prefix_path "${THIRD_PARTY_PATH}/install/gflags")

set(gflags_BUILD_STATIC_LIBS ON)

ExternalProject_Add(
        extern_gtest
        ${EXTERNAL_PROJECT_LOG_ARGS}
        DEPENDS gflags
        GIT_REPOSITORY "https://github.com/google/googletest.git"
        GIT_TAG "release-1.11.0"
        PREFIX ${GTEST_SOURCES_DIR}
        UPDATE_COMMAND ""
        INSTALL_COMMAND ${INSTALL_COMMAND}
        CMAKE_ARGS -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
        -DCMAKE_CXX_FLAGS_RELEASE=${CMAKE_CXX_FLAGS_RELEASE}
        -DCMAKE_CXX_FLAGS_DEBUG=${CMAKE_CXX_FLAGS_DEBUG}
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DCMAKE_C_FLAGS_DEBUG=${CMAKE_C_FLAGS_DEBUG}
        -DCMAKE_C_FLAGS_RELEASE=${CMAKE_C_FLAGS_RELEASE}
        -DCMAKE_INSTALL_PREFIX=${GTEST_INSTALL_DIR}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DBUILD_GMOCK=OFF
        -Dgtest_build_samples=OFF
        -Dgtest_build_tests=OFF
        -DCMAKE_BUILD_TYPE=${THIRD_PARTY_BUILD_TYPE}
        -DCMAKE_PREFIX_PATH=${prefix_path}
)
add_dependencies(extern_gtest gflags)
add_library(gtest STATIC IMPORTED GLOBAL)
set_property(TARGET gtest PROPERTY IMPORTED_LOCATION ${GTEST_LIBRARIES})
add_dependencies(gtest extern_gtest)
link_libraries(gtest gflags)
