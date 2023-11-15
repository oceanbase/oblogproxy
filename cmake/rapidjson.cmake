include(ExternalProject)

set(RAPIDJSON_INSTALL_DIR ${THIRD_PARTY_PATH}/install/rapidjson)

if (NOT EXISTS ${RAPIDJSON_INSTALL_DIR}/include)
    execute_process(COMMAND mkdir -p ${RAPIDJSON_INSTALL_DIR}/include COMMAND_ERROR_IS_FATAL ANY)
endif ()

ExternalProject_Add(
        extern_rapidjson
        ${EXTERNAL_PROJECT_LOG_ARGS}
        GIT_REPOSITORY "https://github.com/Tencent/rapidjson.git"
        GIT_TAG "v1.1.0"
        PREFIX ${THIRD_PARTY_PATH}/rapidjson
        UPDATE_COMMAND ""
        CMAKE_ARGS
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_INSTALL_PREFIX=${RAPIDJSON_INSTALL_DIR}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DRAPIDJSON_BUILD_TESTS=OFF
        -DCMAKE_BUILD_TYPE=${THIRD_PARTY_BUILD_TYPE}
        -DRAPIDJSON_BUILD_EXAMPLES=OFF
        -DRAPIDJSON_BUILD_DOC=OFF
        -DCMAKE_BUILD_TYPE=${THIRD_PARTY_BUILD_TYPE}
)

add_library(rapidjson INTERFACE)
add_dependencies(rapidjson extern_rapidjson)
target_include_directories(rapidjson INTERFACE ${RAPIDJSON_INSTALL_DIR}/include)
include_directories(${RAPIDJSON_INSTALL_DIR}/rapidjson/include)
