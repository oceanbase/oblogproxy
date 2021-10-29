INCLUDE(ExternalProject)

SET(ROCKSDB_SOURCES_DIR ${THIRD_PARTY_PATH}/rocksdb)
SET(ROCKSDB_INSTALL_DIR ${THIRD_PARTY_PATH}/install/rocksdb)
SET(ROCKSDB_INCLUDE_DIR "${ROCKSDB_INSTALL_DIR}/include" CACHE PATH "rocksdb include directory." FORCE)
SET(ROCKSDB_LIBRARIES "${ROCKSDB_INSTALL_DIR}/lib/librocksdb.a" CACHE FILEPATH "rocksdb library." FORCE)
INCLUDE_DIRECTORIES(${ROCKSDB_INCLUDE_DIR})

FILE(WRITE ${ROCKSDB_SOURCES_DIR}/src/build.sh
        "PORTABLE=1 make -j${NUM_OF_PROCESSOR} static_lib"
        )

ExternalProject_Add(
        extern_rocksdb
        ${EXTERNAL_PROJECT_LOG_ARGS}
        DEPENDS gflags zlib snappy bz2
        PREFIX ${ROCKSDB_SOURCES_DIR}
        GIT_REPOSITORY "https://github.com/facebook/rocksdb.git"
        GIT_TAG "v6.3.6"
        UPDATE_COMMAND ""
        CONFIGURE_COMMAND ""
        BUILD_IN_SOURCE 1
        BUILD_COMMAND mv ../build.sh . COMMAND sh build.sh
        INSTALL_COMMAND mkdir -p ${ROCKSDB_INSTALL_DIR}/lib COMMAND cp -r include ${ROCKSDB_INSTALL_DIR}/ COMMAND cp librocksdb.a ${ROCKSDB_LIBRARIES}
)

ADD_DEPENDENCIES(extern_rocksdb lz4 gflags)
ADD_LIBRARY(rocksdb STATIC IMPORTED GLOBAL)
SET_PROPERTY(TARGET rocksdb PROPERTY IMPORTED_LOCATION ${ROCKSDB_LIBRARIES})
ADD_DEPENDENCIES(rocksdb extern_rocksdb)
LINK_LIBRARIES(rocksdb gflags lz4)
