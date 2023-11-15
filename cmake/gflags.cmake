include(ExternalProject)

if (POLICY CMP0097)
    cmake_policy(SET CMP0097 NEW)
endif ()

set(GFLAGS_SOURCES_DIR ${THIRD_PARTY_PATH}/gflags)
set(GFLAGS_INSTALL_DIR ${THIRD_PARTY_PATH}/install/gflags)
set(BUILD_COMMAND $(MAKE) --silent)
set(INSTALL_COMMAND $(MAKE) install)

set(GFLAGS_INCLUDE_DIR "${GFLAGS_INSTALL_DIR}/include")
set(GFLAGS_LIBRARIES "${GFLAGS_INSTALL_DIR}/lib/libgflags.a")

ExternalProject_Add(
        extern_gflags
        ${EXTERNAL_PROJECT_LOG_ARGS}
        GIT_REPOSITORY "https://github.com/gflags/gflags.git"
        GIT_TAG "v2.2.2"
        GIT_SUBMODULES ""
        GIT_SUBMODULES_RECURSE "false"
        PREFIX ${GFLAGS_SOURCES_DIR}
        BUILD_COMMAND ${BUILD_COMMAND}
        INSTALL_COMMAND ${INSTALL_COMMAND}
        UPDATE_COMMAND ""
        CMAKE_ARGS
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DBUILD_STATIC_LIBS=ON
        -DCMAKE_INSTALL_PREFIX=${GFLAGS_INSTALL_DIR}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DGFLAGS_BUILD_TESTING=OFF
        -DGFLAGS_BUILD_PACKAGING=OFF
        -DCMAKE_BUILD_TYPE=${THIRD_PARTY_BUILD_TYPE}
)

if (NOT EXISTS ${GFLAGS_INCLUDE_DIR})
    execute_process(COMMAND mkdir -p ${GFLAGS_INCLUDE_DIR} COMMAND_ERROR_IS_FATAL ANY)
endif ()

add_library(gflags STATIC IMPORTED GLOBAL)
add_dependencies(gflags extern_gflags)
set_target_properties(gflags PROPERTIES IMPORTED_LOCATION ${GFLAGS_LIBRARIES})
target_include_directories(gflags INTERFACE ${GFLAGS_INCLUDE_DIR})
