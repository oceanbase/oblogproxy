include(ExternalProject)

set(JSONCPP_INSTALL_DIR ${THIRD_PARTY_PATH}/install/jsoncpp)

if (NOT EXISTS ${JSONCPP_INSTALL_DIR}/include)
    execute_process(COMMAND mkdir -p ${JSONCPP_INSTALL_DIR}/include COMMAND_ERROR_IS_FATAL ANY)
endif ()

ExternalProject_Add(
        extern_jsoncpp
        ${EXTERNAL_PROJECT_LOG_ARGS}
        GIT_REPOSITORY "https://github.com/open-source-parsers/jsoncpp.git"
        GIT_TAG "1.9.0"
        PREFIX ${THIRD_PARTY_PATH}/jsoncpp
        UPDATE_COMMAND ""
        CMAKE_ARGS
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_INSTALL_PREFIX=${JSONCPP_INSTALL_DIR}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DJSONCPP_WITH_TESTS=OFF
        -DJSONCPP_WITH_POST_BUILD_UNITTEST=OFF
        -DCMAKE_BUILD_TYPE=${THIRD_PARTY_BUILD_TYPE}
)

add_library(jsoncpp STATIC IMPORTED GLOBAL)
add_dependencies(jsoncpp extern_jsoncpp)
set_target_properties(jsoncpp PROPERTIES IMPORTED_LOCATION ${JSONCPP_INSTALL_DIR}/lib64/libjsoncpp.a)
target_include_directories(jsoncpp INTERFACE ${JSONCPP_INSTALL_DIR}/include)
