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

add_library(libmariadb SHARED IMPORTED GLOBAL)
set_target_properties(libmariadb PROPERTIES
        IMPORTED_LOCATION ${DEP_VAR}/usr/local/oceanbase/deps/devel/lib/mariadb/libmariadb.so.3
        IMPORTED_SONAME libmariadb.so)
target_include_directories(libmariadb INTERFACE ${DEP_VAR}/usr/local/oceanbase/deps/devel/include/mariadb)

# install libobcdcce3
set(OB_CE_DEVEL_3_BASE_DIR ${DEP_VAR}/oceanbase-ce-devel-3)
execute_process(
        COMMAND bash deps/dep_create.sh cdc ${DEP_VAR} oceanbase-ce-devel 3 ${OB_CE_DEVEL_3_BASE_DIR}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        COMMAND_ERROR_IS_FATAL ANY)

add_library(libobcdcce3 SHARED IMPORTED GLOBAL)
set_target_properties(libobcdcce3 PROPERTIES
        IMPORTED_LOCATION ${OB_CE_DEVEL_3_BASE_DIR}/usr/lib/libobcdc.so.1.0.0
        IMPORTED_SONAME libobcdc.so.1)
target_include_directories(libobcdcce3 INTERFACE ${OB_CE_DEVEL_3_BASE_DIR}/usr/include)
target_compile_definitions(libobcdcce3
        INTERFACE _OBCDC_H_
        INTERFACE _OBLOG_MSG_)
target_link_libraries(libobcdcce3 INTERFACE libaio)

execute_process(
        COMMAND bash deps/find_dep_config_file.sh
        OUTPUT_VARIABLE OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
message("OUTPUT_VARIABLE:${OUTPUT}")
file(STRINGS ${OUTPUT} file_contents)
set(version_list)

foreach (line IN LISTS file_contents)
    if (NOT line)
        continue()
    endif ()
    string(REGEX MATCH "oceanbase-ce-cdc-([0-9]+\\.[0-9]+\\.[0-9]+)" version_match ${line})
    if (version_match)
        list(APPEND version_list ${CMAKE_MATCH_1})
    endif ()
endforeach ()

list(REMOVE_DUPLICATES version_list)

message("version_list is: ${version_list}")

message("Unique versions:")
foreach (version IN LISTS version_list)
    message(${version})
    # install libobcdc421
    set(OB_CE_CDC_4_BASE_DIR ${DEP_VAR}/oceanbase-ce-cdc-${version})
    execute_process(
            COMMAND bash deps/dep_create.sh cdc ${DEP_VAR} oceanbase-ce-cdc ${version} ${OB_CE_CDC_4_BASE_DIR}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            COMMAND_ERROR_IS_FATAL ANY)

    add_library(libobcdcce${version} SHARED IMPORTED GLOBAL)
    file(REAL_PATH ${OB_CE_CDC_4_BASE_DIR}/home/admin/oceanbase/lib64/libobcdc.so.4 LIBOBCDC_SO EXPAND_TILDE)
    set_target_properties(libobcdcce${version} PROPERTIES
            IMPORTED_LOCATION ${LIBOBCDC_SO}
            IMPORTED_SONAME libobcdc.so.4)
    target_include_directories(libobcdcce${version} INTERFACE ${OB_CE_CDC_4_BASE_DIR}/home/admin/oceanbase/include/libobcdc
            INTERFACE ${OB_CE_CDC_4_BASE_DIR}/home/admin/oceanbase/include/oblogmsg
            INTERFACE ${OB_CE_CDC_4_BASE_DIR}/home/admin/oceanbase/include)
    target_compile_definitions(libobcdcce${version}
            INTERFACE _OBCDC_H_
            INTERFACE _OBCDC_NS_
            INTERFACE _OBLOG_MSG_)
    target_link_libraries(libobcdcce${version}
            INTERFACE libaio
            INTERFACE libmariadb)
endforeach ()

get_target_property(LIBAIO_INCLUDE_PATH libaio INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(LIBAIO_LIBRARIES libaio IMPORTED_LOCATION)
message(STATUS "libaio: ${LIBAIO_INCLUDE_PATH}, ${LIBAIO_LIBRARIES}")

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
