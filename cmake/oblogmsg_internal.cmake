INCLUDE(ExternalProject)

if (POLICY CMP0097)
    cmake_policy(SET CMP0097 NEW)
endif ()

SET(OBLOGMSG_SOURCES_DIR ${THIRD_PARTY_PATH}/oblogmsg)
SET(OBLOGMSG_DOWNLOAD_DIR "${OBLOGMSG_SOURCES_DIR}/src/extern_oblogmsg")
SET(OBLOGMSG_INSTALL_DIR ${THIRD_PARTY_PATH}/install/oblogmsg)
SET(OBLOGMSG_INCLUDE_DIR "${OBLOGMSG_INSTALL_DIR}/include" CACHE PATH "oblogmsg include directory." FORCE)

message("INTERNAL: ${INTERNAL}")
SET(OBLOGMSG_LIB_DIR "${OBLOGMSG_INSTALL_DIR}/lib/" CACHE FILEPATH "oblogmsg library directory." FORCE)
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    SET(OBLOGMSG_LIBRARIES "libdrcmsg.dylib" CACHE FILEPATH "oblogmsg library." FORCE)
else ()
    SET(OBLOGMSG_LIBRARIES "libdrcmsg.so" CACHE FILEPATH "oblogmsg library." FORCE)
endif ()

ExternalProject_Add(
        extern_oblogmsg
        ${EXTERNAL_PROJECT_LOG_ARGS}
        GIT_REPOSITORY "http://gitlab.alibaba-inc.com/fanzhongyang.fzy/LogMessage.git"
        GIT_TAG "enterprise"
        GIT_SUBMODULES ""
        GIT_SUBMODULES_RECURSE "false"
        PREFIX ${OBLOGMSG_SOURCES_DIR}
        BUILD_IN_SOURCE 1
        UPDATE_COMMAND ""
        CMAKE_ARGS -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_BUILD_TYPE=Debug
        -DCMAKE_PREFIX_PATH=${prefix_path}
        -DTEST=OFF
        -DAS_DRCMSG_SHARED=ON
        -DUSE_CXX11_ABI=${USE_CXX11_ABI}
        INSTALL_COMMAND mkdir -p ${OBLOGMSG_INSTALL_DIR}/lib ${OBLOGMSG_INSTALL_DIR}/include COMMAND cp -r ${OBLOGMSG_SOURCES_DIR}/src/extern_oblogmsg/src/${OBLOGMSG_LIBRARIES} ${OBLOGMSG_INSTALL_DIR}/lib/ COMMAND cp -r ${OBLOGMSG_SOURCES_DIR}/src/extern_oblogmsg/include ${OBLOGMSG_INSTALL_DIR}/ COMMAND cp -r ${OBLOGMSG_SOURCES_DIR}/src/extern_oblogmsg/src/${OBLOGMSG_LIBRARIES} ${LIBRARY_OUTPUT_PATH}/
)

ADD_LIBRARY(oblogmsg STATIC IMPORTED GLOBAL)
SET_PROPERTY(TARGET oblogmsg PROPERTY IMPORTED_LOCATION ${OBLOGMSG_LIBRARIES})
ADD_DEPENDENCIES(oblogmsg extern_oblogmsg)
