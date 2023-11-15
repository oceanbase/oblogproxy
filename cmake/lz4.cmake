include(ExternalProject)

set(LZ4_SOURCES_DIR ${THIRD_PARTY_PATH}/lz4)
set(LZ4_INSTALL_DIR ${THIRD_PARTY_PATH}/install/lz4)

ExternalProject_Add(
        extern_lz4
        ${EXTERNAL_PROJECT_LOG_ARGS}
        GIT_REPOSITORY "https://github.com/lz4/lz4.git"
        GIT_TAG "v1.9.3"
        PREFIX ${LZ4_SOURCES_DIR}
        BUILD_IN_SOURCE ON
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_COMMAND $(MAKE) -j${NUM_OF_PROCESSOR} liblz4.a
        INSTALL_COMMAND mkdir -p ${LZ4_INSTALL_DIR} COMMAND cp -r ${LZ4_SOURCES_DIR}/src/extern_lz4/lib ${LZ4_INSTALL_DIR}/
)

if (NOT EXISTS ${LZ4_INSTALL_DIR}/lib)
    execute_process(COMMAND mkdir -p ${LZ4_INSTALL_DIR}/lib COMMAND_ERROR_IS_FATAL ANY)
endif ()

add_library(lz4 STATIC IMPORTED GLOBAL)
add_dependencies(lz4 extern_lz4)
set_target_properties(lz4 PROPERTIES IMPORTED_LOCATION ${LZ4_INSTALL_DIR}/lib/liblz4.a)
target_include_directories(lz4 INTERFACE ${LZ4_INSTALL_DIR}/lib)