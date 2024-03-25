cmake_minimum_required(VERSION 3.20)
if (NOT OBLOGPROXY_VERSION)
    # get the project version from file VERSION.txt
    file(STRINGS ${CMAKE_SOURCE_DIR}/rpm/oblogproxy-version.txt OBLOGPROXY_VERSION
            LIMIT_COUNT 1
            REGEX "^[ \t]*[0-9]+\\.[0-9]+\\.[0-9]+[ \t]*$"
    )
endif ()

option(WITH_ASAN "Compile with AddressSanitizer" OFF)
option(WITH_DEBUG "With debug symbols" ON)
option(WITH_TEST "With Tests" OFF)
option(WITH_DEMO "With Demos" OFF)
option(LOGGER_LEVEL "Log level" 0)
option(WITH_SECURITY_COMPILE "security compile" OFF)
# options for dev
option(WITH_DEPS "With precompiled deps" ON)
option(WITH_US_TIMESTAMP "Enable microseconds start timestamp" ON)

######### global vars ############################################################
set(PROJECT_BUILD_PATH ${CMAKE_CURRENT_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BUILD_PATH})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(DEP_VAR ${PROJECT_BUILD_PATH}/deps)
message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
message(STATUS "DEP_VAR: ${DEP_VAR}")

######### common settings for compilers ############################################################
# output CXX_LIB_DIR, GCC_LIB_DIR that defined gcc libs path
include(env)
project(oblogproxy VERSION ${OBLOGPROXY_VERSION} LANGUAGES CXX)

add_compile_definitions(WITH_US_TIMESTAMP=1)

############## cmake common options #########################################################################
execute_process(
        COMMAND git log -1 --format=%H
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

if (NOT GIT_VERSION)
    message(WARNING "oblogproxy fetch git version empty, use current time as program version")
    string(TIMESTAMP GIT_VERSION "%Y-%m-%d_%H:%M:%S")
endif ()

if (NOT GIT_VERSION)
    message(WARNING "oblogproxy fetch current time failed")
    set(GIT_VERSION "2.0.1")
endif ()
message(STATUS "build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS 0)
set(CMAKE_POSITION_INDEPENDENT_CODE 1)
message(STATUS "oblogproxy version: ${OBLOGPROXY_VERSION}-${GIT_VERSION}")

############## include 3rd #########################################################################
#thread, provides Threads::Threads
include(FindThreads)
set(THIRD_PARTY_PATH ${CMAKE_CURRENT_BINARY_DIR}/third-party)
set(THIRD_PARTY_BUILD_TYPE RelWithDebInfo)
set(EXTERNAL_PROJECT_LOG_ARGS
        LOG_DOWNLOAD 0
        LOG_UPDATE 0
        LOG_CONFIGURE 0
        LOG_BUILD 0
        LOG_TEST 0
        LOG_INSTALL 0)

#===================================> target logproxy_proto for codec_communication <=======================================
# make protoc found libstdc++
set(PROTOC_LINK_FLAGS "-L${CXX_LIB_DIR} -static-libstdc++ -Wl,-z,relro,-z,now")
if (CXX_LIB_DIR)
    set(PROTOC_LINK_FLAGS "${PROTOC_LINK_FLAGS} -L${CXX_LIB_DIR}")
endif ()
if (GCC_LIB_DIR)
    set(PROTOC_LINK_FLAGS "${PROTOC_LINK_FLAGS} -L${GCC_LIB_DIR}")
endif ()

########### libcdc && logmsg #############################################################################################
include(obcdc)
include(lz4)
include(jsoncpp)
include(libevent)
include(protobuf)
include(spdlog)
include(rapidjson)
include(etransfer)
include(logmsg)

file(GLOB PROTO_FILES ${CMAKE_SOURCE_DIR}/proto/*.proto)
message(STATUS "logproxy protos: ${PROTO_FILES}")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto)
foreach (PROTO ${PROTO_FILES})
    message(STATUS "protoc ${PROTO}")
    get_filename_component(PROTO_WE ${PROTO} NAME_WE)
    list(APPEND PROTO_SRCS "${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_WE}.pb.cc")
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_WE}.pb.h ${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_WE}.pb.cc
            COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
            --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/proto
            --proto_path=${CMAKE_SOURCE_DIR}/proto
            ${PROTO}
            DEPENDS protoc)
endforeach ()
add_library(logproxy_proto STATIC ${PROTO_SRCS})
target_include_directories(logproxy_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/proto)
target_link_libraries(logproxy_proto PUBLIC protobuf)

#=====================================> sql parser <===========================================
# Build sql-parser library
set(PARSER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/sqlparser)
set(PARSER_BUILD_COMMAND $(MAKE) clean && $(MAKE) CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} FLEX=${DEP_VAR}/usr/local/oceanbase/devtools/bin/flex)
add_custom_target(parser_build ALL COMMAND ${PARSER_BUILD_COMMAND} WORKING_DIRECTORY ${PARSER_DIR})
add_library(sqlparser STATIC IMPORTED GLOBAL)
add_dependencies(sqlparser parser_build)
set_target_properties(sqlparser PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/src/sqlparser/libsqlparser.a)
target_include_directories(sqlparser INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/sqlparser/src)

#===================================> define new targets <=======================================
#===== common =====
file(GLOB COMMON_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/codec/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/communication/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/metric/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/obaccess/*.cpp
)
add_library(common STATIC ${COMMON_SRC})
target_compile_definitions(common PUBLIC __OMS_VERSION__=\"${OBLOGPROXY_VERSION}-${GIT_VERSION}\")
target_include_directories(common
        PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/proto
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/common/
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/logmsg/
)
include(gflags)
find_package(ZLIB REQUIRED)
target_link_libraries(common
        PUBLIC logproxy_proto
        PUBLIC logmsg # for oblogmsg
        PUBLIC libevent::core
        PUBLIC libevent::pthreads
        PUBLIC libevent::extra
        PUBLIC jsoncpp
        PUBLIC rapidjson
        PUBLIC lz4
        PUBLIC OpenSSL::ssl
        PUBLIC Threads::Threads
        PUBLIC spdlog
        PUBLIC sqlparser
        PUBLIC ZLIB::ZLIB
)
target_compile_definitions(common PUBLIC _OBLOG_MSG_)

get_target_property(COMMON_INCLUDE_PATH common INCLUDE_DIRECTORIES)
get_target_property(COMMON_DEP_INCLUDE_PATH common INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "COMMON_INCLUDE_PATH: ${COMMON_INCLUDE_PATH}, COMMON_DEP_INCLUDE_PATH: ${COMMON_DEP_INCLUDE_PATH}")
message(STATUS "COMMON_SRC: ${COMMON_SRC}")

#======= obcdc wrapper =======
function(add_obcdc_access_library OB_CDC_TARGET_NAME LIBOBCDC_TARGET_NAME)
    add_library(${OB_CDC_TARGET_NAME} SHARED src/obcdcaccess/obcdc/obcdc_access.cpp)
    target_include_directories(${OB_CDC_TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/obcdcaccess/obcdc)
    target_link_libraries(${OB_CDC_TARGET_NAME}
            PRIVATE ${LIBOBCDC_TARGET_NAME}
            PRIVATE common
    )
    set_target_properties(${OB_CDC_TARGET_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN"
            OUTPUT_NAME "obcdc"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${OB_CDC_TARGET_NAME}")
endfunction()

# obcdc_access
message(STATUS "build with oceanbase community")
add_obcdc_access_library(obcdc-ce-3 libobcdcce3)
execute_process(
        COMMAND bash deps/find_dep_config_file.sh
        OUTPUT_VARIABLE OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
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

foreach (version IN LISTS version_list)
    add_obcdc_access_library(obcdc-ce-${version} libobcdcce${version})
endforeach ()

# obcdc_base
add_library(obcdc_base STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/obcdcaccess/obcdc_factory.cpp)
target_include_directories(obcdc_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/obcdcaccess/)
target_link_libraries(obcdc_base
        PRIVATE common
        PRIVATE dl)
target_compile_definitions(obcdc_base PRIVATE _COMMUNITY_)

#===== oblogreader =====
file(GLOB OBLOGREADER_SRC ./src/oblogreader/*.cpp)
add_library(oblogreader_static STATIC ${OBLOGREADER_SRC})
set_target_properties(oblogreader_static PROPERTIES OUTPUT_NAME "oblogreader")
target_include_directories(oblogreader_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/oblogreader/)
target_link_libraries(oblogreader_static
        PUBLIC common
        PRIVATE obcdc_base)

# target mysql_protocol
add_library(mysql_protocol INTERFACE)
target_include_directories(mysql_protocol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/mysql-protocol)

# ===== target ob_binlog_server =====
add_library(ddl_converter STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/ddl-converter/ddl_converter.cpp
)
target_link_libraries(ddl_converter
        PUBLIC common
        PUBLIC etransfer
)
target_include_directories(ddl_converter
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/ddl-converter
)

# binlog converter
add_library(binlog_converter_static STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_converter/binlog_converter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_converter/clog_reader_routine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_converter/binlog_convert.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_converter/binlog_storage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_converter/table_cache.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/ob_log_event.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_index.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/data_type.cpp
)
target_include_directories(binlog_converter_static
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_converter)
target_link_libraries(binlog_converter_static
        PRIVATE mysql_protocol
        PUBLIC common
        PUBLIC ddl_converter
        PRIVATE obcdc_base
        PRIVATE rapidjson
        PRIVATE libevent::pthreads
)

# binlog server
add_library(ob_binlog_server STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_dumper.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_index.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_server.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_state_machine.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/connection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/data_type.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/env.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/event_dispatch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/event_wrapper.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/fork_thread.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/ob_log_event.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/thread_pool_executor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/cmd_processor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/sql_cmd_processor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/sql_parser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_func.cpp
)
target_include_directories(ob_binlog_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog)
target_link_libraries(ob_binlog_server
        PRIVATE mysql_protocol
        PRIVATE common
        PRIVATE rapidjson
        PRIVATE libevent::pthreads
        PRIVATE binlog_converter_static
)

#===== logproxy static =====
file(GLOB LOGPROXY_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/arranger/*.cpp)
add_library(logproxy_static STATIC ${LOGPROXY_SRC})
set_target_properties(logproxy_static PROPERTIES OUTPUT_NAME "logproxy")
target_include_directories(logproxy_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/arranger/)
target_link_libraries(logproxy_static PRIVATE common)
get_target_property(LOGPROXY_STATIC_INCLUDE_PATH logproxy_static INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "LOGPROXY_STATIC_INCLUDE_PATH: ${LOGPROXY_STATIC_INCLUDE_PATH}")

if (WITH_DEMO)
    # demo client
    file(GLOB DEMO_CLIENT_SRC ./src/demo/client_demo.cpp)
    add_executable(demo_client ${DEMO_CLIENT_SRC})
    add_dependencies(demo_client common)
    target_include_directories(demo_client PUBLIC ${COMMON_INC})
    target_link_libraries(demo_client
            PRIVATE common
    )
    target_link_options(demo_client PUBLIC -static-libstdc++ ${ASAN_LINK_OPTION})
endif ()

if (WITH_TEST)
    include(gtest)
    # target test_base
    message(STATUS "target test_base")
    add_executable(test_base
            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_entry.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_binlog_event_convert.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_binlog_event.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_data_type.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_index_file.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_thread_pool_executor.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_sql_parser.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_defer.cpp
            #            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_aes.cpp
            #            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_codec.cpp
            #            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_compress.cpp
            #            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_conf.cpp
            #            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_http.cpp
            #            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_message_buffer.cpp
            #            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_net.cpp
            #            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_ob_mysql.cpp
            #            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_ob_sha1.cpp
            #            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_queue.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_binlog_dumper.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_json_parse.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/test/test_sys_metric.cpp)
    target_include_directories(test_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/test)
    target_link_libraries(test_base
            PRIVATE logproxy_static
            PRIVATE binlog_converter_static
            PRIVATE ob_binlog_server
    )
    target_link_options(test_base PUBLIC -static-libstdc++ ${ASAN_LINK_OPTION})
endif ()

#===== oblogreader =====
add_executable(oblogreader ${CMAKE_CURRENT_SOURCE_DIR}/src/oblogreader/oblogreader_entry.cpp)
target_link_options(oblogreader PRIVATE -rdynamic)
target_link_libraries(oblogreader PRIVATE oblogreader_static)
get_target_property(OBLOGREADER_INCLUDE_PATH oblogreader INCLUDE_DIRECTORIES)
message(STATUS "OBLOGREADER_OUTPUT_PATH: ${EXECUTABLE_OUTPUT_PATH}, OBLOGREADER_INCLUDE_PATH: " ${OBLOGREADER_INCLUDE_PATH})

#===== binlog converter =====
add_executable(binlog_converter ${CMAKE_CURRENT_SOURCE_DIR}/src/binlog/binlog_converter/binlog_converter_entry.cpp)
target_link_options(binlog_converter PRIVATE -rdynamic)
target_link_libraries(binlog_converter PRIVATE binlog_converter_static)
get_target_property(BINLOG_CONVERTER_INCLUDE_PATH binlog_converter INCLUDE_DIRECTORIES)
message(STATUS "BINLOG_CONVERTER_OUTPUT_PATH: ${EXECUTABLE_OUTPUT_PATH}, BINLOG_CONVERTER_INCLUDE_PATH: " ${BINLOG_CONVERTER_INCLUDE_PATH})

#===== main =====
add_executable(logproxy ${CMAKE_CURRENT_SOURCE_DIR}/src/entry.cpp)
target_link_libraries(logproxy
        PRIVATE common
        PRIVATE logproxy_static
        PRIVATE ob_binlog_server
)
get_target_property(LOGPROXY_INCLUDE_PATH logproxy INCLUDE_DIRECTORIES)
message(STATUS "LOGPROXY_OUTPUT_PATH: ${EXECUTABLE_OUTPUT_PATH}, LOGPROXY_INCLUDE_PATH: ${LOGPROXY_INCLUDE_PATH}")

include(rpm)