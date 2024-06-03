include(ExternalProject)

set(HTTPLIB_PREFIX "${THIRD_PARTY_PATH}/httplib")
set(HTTPLIB_INCLUDE_DIR "${HTTPLIB_PREFIX}/src/httplib")

ExternalProject_Add(httplib
        PREFIX "${HTTPLIB_PREFIX}"
        GIT_REPOSITORY "https://github.com/yhirose/cpp-httplib.git"
        GIT_TAG "v0.15.3"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        LOG_DOWNLOAD ON
)

ExternalProject_Get_Property(httplib SOURCE_DIR)
set(HTTPLIB_DIR ${SOURCE_DIR} CACHE INTERNAL "Path to include folder for httplib")
set(HTTPLIB_INCLUDE_DIR ${HTTPLIB_DIR})
