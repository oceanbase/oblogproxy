# header-only target
add_library(logmsg_wrapper INTERFACE)
target_include_directories(logmsg_wrapper INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/obcdcaccess/logmsg)

# install tools
execute_process(
        COMMAND bash deps/dep_create.sh tool ${DEP_VAR}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND_ERROR_IS_FATAL ANY)
add_library(libaio SHARED IMPORTED GLOBAL)
set_target_properties(libaio PROPERTIES
        IMPORTED_LOCATION ${DEP_VAR}/usr/local/oceanbase/deps/devel/lib/libaio.so.1.0.1
        IMPORTED_SONAME libaio.so.1)
target_include_directories(libaio INTERFACE ${DEP_VAR}/usr/local/oceanbase/deps/devel/include)

# oblogmsg wrapper
add_library(oblogmsg SHARED IMPORTED GLOBAL)
set_target_properties(oblogmsg PROPERTIES IMPORTED_LOCATION ${DEP_VAR}/usr/local/oceanbase/deps/devel/lib/liboblogmsg.a)
target_include_directories(oblogmsg INTERFACE ${DEP_VAR}/usr/local/oceanbase/deps/devel/include/oblogmsg)
target_link_libraries(logmsg_wrapper INTERFACE oblogmsg)

get_target_property(OBLOGMSG_INCLUDE_PATH oblogmsg INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(OBLOGMSG_LOCATION oblogmsg IMPORTED_LOCATION)
message(STATUS "OBLOGMSG_INCLUDE_PATH: ${OBLOGMSG_INCLUDE_PATH}, OBLOGMSG_LOCATION: ${OBLOGMSG_LOCATION}")

# install libobcdcce3
set(OB_CE_DEVEL_BASE_DIR ${DEP_VAR}/oceanbase-ce-devel)
execute_process(
        COMMAND bash deps/dep_create.sh cdc ${DEP_VAR} oceanbase-ce-devel 3 ${OB_CE_DEVEL_BASE_DIR}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND_ERROR_IS_FATAL ANY)

add_library(libobcdcce3 SHARED IMPORTED GLOBAL)
set_target_properties(libobcdcce3 PROPERTIES
        IMPORTED_LOCATION ${OB_CE_DEVEL_BASE_DIR}/usr/lib/libobcdc.so.1.0.0
        IMPORTED_SONAME libobcdc.so.1)
target_include_directories(libobcdcce3 INTERFACE ${OB_CE_DEVEL_BASE_DIR}/usr/include)
target_compile_definitions(libobcdcce3
        INTERFACE _OBCDC_H_
        INTERFACE _OBLOG_MSG_)
target_link_libraries(libobcdcce3 INTERFACE libaio)

# install libobcdcce4
set(OB_CE_CDC_BASE_DIR ${DEP_VAR}/oceanbase-ce-cdc)
execute_process(
        COMMAND bash deps/dep_create.sh cdc ${DEP_VAR} oceanbase-ce-cdc 4 ${OB_CE_CDC_BASE_DIR}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND_ERROR_IS_FATAL ANY)

add_library(libobcdcce4 SHARED IMPORTED GLOBAL)
set_target_properties(libobcdcce4 PROPERTIES
        IMPORTED_LOCATION ${OB_CE_CDC_BASE_DIR}/home/admin/oceanbase/lib64/libobcdc.so.4.2.1.0
        IMPORTED_SONAME libobcdc.so.4)
target_include_directories(libobcdcce4 INTERFACE ${OB_CE_CDC_BASE_DIR}/home/admin/oceanbase/include/libobcdc)
target_compile_definitions(libobcdcce4
        INTERFACE _OBCDC_H_
        INTERFACE _OBCDC_NS_
        INTERFACE _OBLOG_MSG_)

add_library(libmariadb SHARED IMPORTED GLOBAL)
set_target_properties(libmariadb PROPERTIES
        IMPORTED_LOCATION ${DEP_VAR}/usr/local/oceanbase/deps/devel/lib/mariadb/libmariadb.so.3
        IMPORTED_SONAME libmariadb.so)
target_include_directories(libmariadb INTERFACE ${DEP_VAR}/usr/local/oceanbase/deps/devel/include/mariadb)
target_link_libraries(libobcdcce4
        INTERFACE libaio
        INTERFACE libmariadb)

get_target_property(LIBAIO_INCLUDE_PATH libaio INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(LIBAIO_LIBRARIES libaio IMPORTED_LOCATION)
message(STATUS "libaio: ${LIBAIO_INCLUDE_PATH}, ${LIBAIO_LIBRARIES}")

get_target_property(LOGMSG_WRAPPER_INCLUDE_PATH logmsg_wrapper INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(LOGMSG_WRAPPER_DEPS logmsg_wrapper INTERFACE_LINK_LIBRARIES)
message(STATUS "logmsg_wrapper: ${LOGMSG_WRAPPER_INCLUDE_PATH}, deps: ${LOGMSG_WRAPPER_DEPS}")

###################### deps
cmake_policy(SET CMP0074 NEW)
set(OpenSSL_ROOT ${DEP_VAR}/usr/local/oceanbase/deps/devel)

if (NOT EXISTS ${OpenSSL_ROOT}/include)
    execute_process(COMMAND mkdir -p ${OpenSSL_ROOT}/include COMMAND_ERROR_IS_FATAL ANY)
endif ()

add_library(OpenSSL::crypto STATIC IMPORTED GLOBAL)
set_target_properties(OpenSSL::crypto PROPERTIES IMPORTED_LOCATION ${OpenSSL_ROOT}/lib/libcrypto.a)
target_include_directories(OpenSSL::crypto INTERFACE ${OpenSSL_ROOT}/include)
target_link_libraries(OpenSSL::crypto INTERFACE dl)

get_target_property(CRYPTO_INCLUDE_PATH OpenSSL::crypto INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(CRYPTO_LIBRARIES OpenSSL::crypto IMPORTED_LOCATION)
message(STATUS "crypto: ${CRYPTO_INCLUDE_PATH}, ${CRYPTO_LIBRARIES}")

add_library(OpenSSL::ssl STATIC IMPORTED GLOBAL)
set_target_properties(OpenSSL::ssl PROPERTIES IMPORTED_LOCATION ${OpenSSL_ROOT}/lib/libssl.a)
target_include_directories(OpenSSL::ssl INTERFACE ${OpenSSL_ROOT}/include)
target_link_libraries(OpenSSL::ssl INTERFACE OpenSSL::crypto)

get_target_property(OPENSSL_INCLUDE_PATH OpenSSL::ssl INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(OPENSSL_LIBRARIES OpenSSL::ssl IMPORTED_LOCATION)
message(STATUS "openssl: ${OPENSSL_INCLUDE_PATH}, ${OPENSSL_LIBRARIES}")