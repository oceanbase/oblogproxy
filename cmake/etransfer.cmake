include(ExternalProject)

set(ETRANSFER_LOCAL_ROOT ${THIRD_PARTY_PATH}/etransfer)

set(ETRANSFER_SOURCE_DIR ${ETRANSFER_LOCAL_ROOT}/src/extern_etransfer)

set(ETRANSFER_INSTALL_DIR ${THIRD_PARTY_PATH}/install/etransfer)

ExternalProject_Add(
        extern_etransfer
        PREFIX ${ETRANSFER_LOCAL_ROOT}
        URL ${CMAKE_SOURCE_DIR}/third-party/etransfer-cpp
        DOWNLOAD_DIR ${ETRANSFER_LOCAL_ROOT}
        SOURCE_DIR ${ETRANSFER_SOURCE_DIR}
        INSTALL_DIR ${ETRANSFER_INSTALL_DIR}
        CMAKE_CACHE_ARGS
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
        -DCMAKE_SOURCE_DIR:PATH=<SOURCE_DIR>
        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
)

if (NOT EXISTS ${ETRANSFER_INSTALL_DIR}/include)
    execute_process(COMMAND mkdir -p ${ETRANSFER_INSTALL_DIR}/include COMMAND_ERROR_IS_FATAL ANY)
endif ()

add_library(etransfer STATIC IMPORTED GLOBAL)
add_dependencies(etransfer extern_etransfer)
set_target_properties(etransfer PROPERTIES IMPORTED_LOCATION ${ETRANSFER_INSTALL_DIR}/lib/libetransfer.a)
target_include_directories(etransfer INTERFACE ${ETRANSFER_INSTALL_DIR}/include)