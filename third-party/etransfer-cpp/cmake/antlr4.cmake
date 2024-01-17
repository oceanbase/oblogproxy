include(ExternalProject)

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif ()

# only jre required
find_package(Java 11.0 COMPONENTS Runtime REQUIRED)

set(ANTLR4_LOCAL_ROOT ${PROJECT_SOURCE_DIR}/thirdparty/antlr4)

set(ANTLR4_EXTERNAL_ROOT ${THIRD_PARTY_PATH}/antlr4)

set(ANTLR4_SOURCE_DIR ${ANTLR4_EXTERNAL_ROOT}/src/extern_antlr4)

set(ANTLR4_INSTALL_DIR ${THIRD_PARTY_PATH}/install/antlr4)

set(ANTLR4_ZIP_REPOSITORY https://github.com/antlr/website-antlr4/raw/gh-pages/download/antlr4-cpp-runtime-4.13.1-source.zip)

set(ANTLR4_ZIP_MD5 c875c148991aacd043f733827644a76f)

set(ANTLR4_JAR_URL https://github.com/antlr/website-antlr4/raw/gh-pages/download/antlr-4.13.1-complete.jar)

set(ANTLR4_JAR_MD5 78af96af276609af0bfb3f1e2bfaef89)

set(ANTLR4_JAR_DOWNLOAD_DIR ${ANTLR4_EXTERNAL_ROOT}/antlr-4.13.1-complete.jar)

file(COPY
        ${ANTLR4_LOCAL_ROOT}/antlr-4.13.1-complete.jar
        DESTINATION
        ${ANTLR4_EXTERNAL_ROOT}
)

file(COPY
        ${ANTLR4_LOCAL_ROOT}/antlr4-cpp-runtime-4.13.1-source.zip
        DESTINATION
        ${ANTLR4_EXTERNAL_ROOT}
)

file(DOWNLOAD
        ${ANTLR4_JAR_URL}
        ${ANTLR4_JAR_DOWNLOAD_DIR}
        EXPECTED_HASH
        MD5=${ANTLR4_JAR_MD5}
)

ExternalProject_Add(
        extern_antlr4
        BUILD_BYPRODUCTS ${ANTLR4_INSTALL_DIR}/lib/libantlr4-runtime.a
        PREFIX ${ANTLR4_EXTERNAL_ROOT}
        URL ${ANTLR4_ZIP_REPOSITORY}
        URL_MD5 ${ANTLR4_ZIP_MD5}
        DOWNLOAD_DIR ${ANTLR4_EXTERNAL_ROOT}
        SOURCE_DIR ${ANTLR4_SOURCE_DIR}
        INSTALL_DIR ${ANTLR4_INSTALL_DIR}
        CMAKE_CACHE_ARGS
        -DCMAKE_BUILD_TYPE:STRING=Release
        -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
        -DCMAKE_SOURCE_DIR:PATH=<SOURCE_DIR>
        -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
        -DCMAKE_INSTALL_LIBDIR:PATH=<INSTALL_DIR>/lib
        -DANTLR_BUILD_CPP_TESTS:BOOL=OFF
        -DANTLR_BUILD_SHARED:BOOL=OFF
        -DWITH_DEMO:STRING=False
)

if (NOT EXISTS ${ANTLR4_INSTALL_DIR}/include/antlr4-runtime)
    execute_process(COMMAND mkdir -p ${ANTLR4_INSTALL_DIR}/include/antlr4-runtime COMMAND_ERROR_IS_FATAL ANY)
endif ()

add_library(antlr4 STATIC IMPORTED GLOBAL)
add_dependencies(antlr4 extern_antlr4)
set_target_properties(antlr4 PROPERTIES IMPORTED_LOCATION ${ANTLR4_INSTALL_DIR}/lib/libantlr4-runtime.a)
target_include_directories(antlr4 INTERFACE ${ANTLR4_INSTALL_DIR}/include/antlr4-runtime)