cmake_minimum_required(VERSION 3.2)
project(oblogproxy CXX)

macro(ob_define VAR DEFAULT)
    if (NOT DEFINED ${VAR})
        set(${VAR} ${DEFAULT})
    endif ()
endmacro()

ob_define(OBLOGPROXY_RELEASEID 1)

option(WITH_DEBUG "With debug symbols" ON)
option(WITH_ASAN "Compile with AddressSanitizer" OFF)
option(WITH_TEST "With Tests" OFF)
option(WITH_DEMO "With Demos" OFF)
option(WITH_JNI_LIB "With oblogreader jni lib" OFF)
option(WITH_GLOG "With google log" ON)
option(WITH_DEPS "With precompiled deps" ON)
option(WITH_LOGMSG "compiled logmsg from source files" OFF)
option(WITH_US_TIMESTAMP "Enable microseconds start timestamp" ON)
option(USE_OBCDC_NS "With libobcdc" ON)
option(USE_LIBOBLOG "With precompiled liboblog" OFF)
option(USE_CXX11_ABI "Build with C++11 ABI" OFF)
option(USE_LIBOBLOG_3 "With liboblog 3.x" OFF)

SET(OMS_PROJECT_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR})
SET(OMS_PROJECT_BUILD_PATH ${CMAKE_CURRENT_BINARY_DIR})
SET(EXECUTABLE_OUTPUT_PATH ${OMS_PROJECT_BUILD_PATH})
SET(LIBRARY_OUTPUT_PATH ${OMS_PROJECT_BUILD_PATH})

SET(DEP_VAR $ENV{DEP_VAR})
SET(JAVA_HOME $ENV{JAVA_HOME})
message(STATUS "DEP_VAR: ${DEP_VAR}")
message(STATUS "JAVA_HOME: ${JAVA_HOME}")

SET(FIND_LIBOBLOG ON)
if (USE_OBCDC_NS)
    SET(OBCDC_NAME "libobcdc")
    SET(OBCDC_NAME_VAR "-DUSE_OBCDC_NS")
    if (USE_LIBOBLOG_3)
        SET(OBCDC_NAME_VAR "${OBCDC_NAME_VAR} -DUSE_LIBOBLOG_3")
    endif()
else ()
    SET(OBCDC_NAME "liboblog")
endif ()

if (WITH_DEPS)
    SET(DEP_VAR ${OMS_PROJECT_BUILD_PATH}/deps)
    SET(THIRD_LIB_DIR ${DEP_VAR}/usr/local/oceanbase/deps/devel/lib)

    SET(LIBOBLOG_RPM_NAME "oceanbase-ce-cdc")
    if (NOT USE_LIBOBLOG)
        SET(FIND_LIBOBLOG OFF)

        if (USE_LIBOBLOG_3)
            # version before 4.0
            SET(LIBOBLOG_RPM_NAME "oceanbase-ce-devel")
            SET(LIBOBLOG_INCLUDE_PATH ${DEP_VAR}/usr/include)
            SET(LIBOBLOG_LIBRARIES ${DEP_VAR}/usr/lib/${OBCDC_NAME}.so)
        else()
            SET(LIBOBLOG_INCLUDE_PATH "${DEP_VAR}/home/admin/oceanbase/include")
            SET(LIBOBLOG_LIBRARIES ${DEP_VAR}/home/admin/oceanbase/lib64/${OBCDC_NAME}.so)
            SET(LIBOBLOG_LIB_DIR ${DEP_VAR}/home/admin/oceanbase/lib64/)

            SET(LIBOBLOG_DEPS
                    ${THIRD_LIB_DIR}/libaio.so
                    ${THIRD_LIB_DIR}/mariadb/libmariadb.so
                    )
        endif()

    endif ()

    execute_process(
            COMMAND bash deps/dep_create.sh ${OMS_PROJECT_BUILD_PATH}/deps ${LIBOBLOG_RPM_NAME}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            COMMAND_ERROR_IS_FATAL ANY
    )

    # make openssl found
    CMAKE_POLICY(SET CMP0074 NEW)
    SET(OpenSSL_ROOT ${DEP_VAR}/usr/local/oceanbase/deps/devel/)

    # compiler
    SET(COMPILER_DIR ${DEP_VAR}/usr/local/oceanbase/devtools/bin/)
else ()
    # compiler
    execute_process(
            COMMAND which gcc
            OUTPUT_VARIABLE GCC_BIN
    )
    GET_FILENAME_COMPONENT(COMPILER_DIR ${GCC_BIN} DIRECTORY)
endif ()

message(STATUS "COMPILER_DIR: ${COMPILER_DIR}")

find_program(CC NAMES gcc PATHS ${COMPILER_DIR} /usr/bin/ NO_DEFAULT_PATH)
find_program(CXX NAMES g++ PATHS ${COMPILER_DIR} /usr/bin/ NO_DEFAULT_PATH)
find_program(AR NAMES gcc-ar ar PATHS ${COMPILER_DIR} /usr/bin/ NO_DEFAULT_PATH)
SET(CMAKE_C_COMPILER ${CC})
SET(CMAKE_CXX_COMPILER ${CXX})
SET(CMAKE_C_COMPILER_AR ${AR})
SET(CMAKE_CXX_COMPILER_AR ${AR})
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "AR compiler: ${CMAKE_C_COMPILER_AR}")

GET_FILENAME_COMPONENT(COMPILER_DIR ${CMAKE_C_COMPILER} DIRECTORY)
GET_FILENAME_COMPONENT(COMPILER_BASE_DIR ${COMPILER_DIR} DIRECTORY)
SET(CXX_LIB_DIR ${COMPILER_BASE_DIR}/lib64/)

# make protoc found libstdc++
SET(PROTOC_LINK_FLAGS "-L${CXX_LIB_DIR} -static-libstdc++")
if (THIRD_LIB_DIR)
    SET(PROTOC_LINK_FLAGS "${PROTOC_LINK_FLAGS} -L${THIRD_LIB_DIR}")
endif ()

SET(DEP_LIB_PATH ${THIRD_LIB_DIR} ${CXX_LIB_DIR})

if (NOT USE_CXX11_ABI)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=0")
endif ()

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

SET(THIRD_PARTY_PATH ${CMAKE_CURRENT_BINARY_DIR}/third-party)
SET(THIRD_PARTY_BUILD_TYPE Release)
SET(EXTERNAL_PROJECT_LOG_ARGS
        LOG_DOWNLOAD 0
        LOG_UPDATE 0
        LOG_CONFIGURE 0
        LOG_BUILD 0
        LOG_TEST 0
        LOG_INSTALL 0)

INCLUDE(ProcessorCount)
ProcessorCount(NUM_OF_PROCESSOR)
message(NUM_OF_PROCESSOR: ${NUM_OF_PROCESSOR})

#thread
include(FindThreads)

#openssl
find_package(OpenSSL REQUIRED)

message(STATUS "ssl:" ${OPENSSL_SSL_LIBRARY})
message(STATUS "crypto:" ${OPENSSL_CRYPTO_LIBRARY})

ADD_LIBRARY(ssl SHARED IMPORTED GLOBAL)
SET_PROPERTY(TARGET ssl PROPERTY IMPORTED_LOCATION ${OPENSSL_SSL_LIBRARY})

ADD_LIBRARY(crypto SHARED IMPORTED GLOBAL)
SET_PROPERTY(TARGET crypto PROPERTY IMPORTED_LOCATION ${OPENSSL_CRYPTO_LIBRARY})

if (WITH_JNI_LIB)
    SET(JAVA_INCLUDE_DIR ${JAVA_HOME}/include ${JAVA_HOME}/include/linux ${JAVA_HOME}/include/darwin)
    SET(JAVA_LIB_DIR ${JAVA_HOME}/jre/lib)
else ()
    SET(JAVA_INCLUDE_DIR "")
    SET(JAVA_LIB_DIR "")
endif ()

include(lz4)
include(jsoncpp)
include(libevent)

if (WITH_GLOG)
    include(gflags)
    include(glog)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWITH_GLOG=1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWITH_GLOG=1")
else ()
    SET(GFLAGS_INCLUDE_DIR "")
    SET(GLOG_INCLUDE_DIR "")
    SET(GFLAGS_LIBRARIES "")
    SET(GLOG_LIBRARIES "")
endif ()

if (WITH_TEST)
    include(gtest)
else ()
    SET(GTEST_INCLUDE_DIR "")
    SET(GTEST_LIBRARIES "")
endif ()

#protobuf
include(protobuf)

file(GLOB PROTO_FILES ${CMAKE_SOURCE_DIR}/proto/*.proto)
message("protoc: ${PROTOBUF_PROTOC_EXECUTABLE}, proto inc: ${PROTOBUF_INCLUDE_DIRS}, lib: ${PROTOBUF_LIBRARIES}, ${PROTOBUF_PROTOC_LIBRARY}, protos: ${PROTO_FILES}")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto)
foreach (PROTO ${PROTO_FILES})
    message(proto : ${PROTO})
    get_filename_component(PROTO_WE ${PROTO} NAME_WE)
    list(APPEND PROTO_HDRS "${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_WE}.pb.h")
    list(APPEND PROTO_SRCS "${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_WE}.pb.cc")
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_WE}.pb.h ${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_WE}.pb.cc
            COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
            --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/proto
            --proto_path=${PROTOBUF_INCLUDE_DIR}
            --proto_path=${CMAKE_SOURCE_DIR}/proto ${PROTO}
            DEPENDS protobuf
    )
endforeach ()
add_library(PROTO_OBJS OBJECT ${PROTO_SRCS} ${PROTO_HDRS})
message("protoc: ${PROTOBUF_PROTOC_EXECUTABLE}, proto srcs : ${PROTO_SRCS}")


# oblog
if (FIND_LIBOBLOG)
    find_path(LIBOBLOG_INCLUDE_PATH NAMES ${OBCDC_NAME}.h ${OBCDC_NAME}/${OBCDC_NAME}.h)
    find_library(LIBOBLOG_LIBRARIES NAMES ${OBCDC_NAME}.so)
    if ((NOT LIBOBLOG_INCLUDE_PATH) OR (NOT LIBOBLOG_LIBRARIES))
        message(FATAL_ERROR "Fail to find ${OBCDC_NAME}")
    endif ()
endif ()

option(LOGMSG_BY_LIBOBLOG "use logmsg provide by liboblog" ON)
set(LOGMSG_BY_LIBOBLOG_DEFINE "")
if (LOGMSG_BY_LIBOBLOG)
    set(LOGMSG_BY_LIBOBLOG_DEFINE "-DLOGMSG_BY_LIBOBLOG=1 -DOB_BUILD_OPENSOURCE=1")
    SET(OBLOGMSG_MAPPING "${LOGMSG_BY_LIBOBLOG_DEFINE} -DLogMsgLocalInit=\"if((_t_s_lmb=new(std::nothrow)LogMsgBuf())==nullptr){OMS_ERROR<<\\\"Failed to alloc LogMsgBuf\\\";stop();return;}\" -DLogMsgLocalDestroy=\"delete _t_s_lmb\"")
    SET(OBLOGMSG_INCLUDE_DIR ${LIBOBLOG_INCLUDE_PATH} ${LIBOBLOG_INCLUDE_PATH}/oblogmsg ${LIBOBLOG_INCLUDE_PATH}/../oblogmsg)

    SET(OBLOGMSG_LIBRARIES ${LIBOBLOG_LIBRARIES})
    GET_FILENAME_COMPONENT(OBLOGMSG_LIB_DIR ${OBLOGMSG_LIBRARIES} DIRECTORY)

    ADD_LIBRARY(oblogmsg STATIC IMPORTED GLOBAL)
    SET_PROPERTY(TARGET oblogmsg PROPERTY IMPORTED_LOCATION ${OBLOGMSG_LIBRARIES})

else ()
    include(oblogmsg)
endif ()

GET_FILENAME_COMPONENT(LIBOBLOG_INCLUDE_PATH_NAME ${LIBOBLOG_INCLUDE_PATH} NAME)
if (${LIBOBLOG_INCLUDE_PATH_NAME} STREQUAL "${OBCDC_NAME}")
    set(LIBOBLOG_INCLUDE_PATH ${LIBOBLOG_INCLUDE_PATH} ${LIBOBLOG_INCLUDE_PATH}/..)
else ()
    set(LIBOBLOG_INCLUDE_PATH ${LIBOBLOG_INCLUDE_PATH} ${LIBOBLOG_INCLUDE_PATH}/${OBCDC_NAME})
endif ()

if (WITH_US_TIMESTAMP)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWITH_US_TIMESTAMP")
endif()

message("oblogmsg: ${OBLOGMSG_INCLUDE_DIR}, ${OBLOGMSG_LIBRARIES}")
message("liboblog: ${LIBOBLOG_INCLUDE_PATH}, ${LIBOBLOG_LIBRARIES}, ${LIBOBLOG_DEPS}")

execute_process(
        COMMAND git log -1 --format=%H
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

if (NOT GIT_VERSION)
    message(WARNING "oblogproxy fetch git version empty, use current time as program version")
    STRING(TIMESTAMP GIT_VERSION "%Y-%m-%d_%H:%M:%S")
endif ()

if (NOT GIT_VERSION)
    message(WARNING "oblogproxy fetch current time failed")
    SET(GIT_VERSION "2.0.0")
endif ()

message("oblogproxy version: ${GIT_VERSION}")

if (WITH_DEBUG)
    SET(DEBUG_SYMBOL "-ggdb")
else ()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDEBUG")
endif ()

if (WITH_ASAN)
    SET(ASAN_COMPILE_OPTION "-fsanitize=address -fno-omit-frame-pointer")
    SET(ASAN_LINK_OPTION "-fsanitize=address")
endif ()

######### get arch ###########################################
execute_process(COMMAND uname -m
        OUTPUT_VARIABLE ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
        )
set(OS_ARCH ${ARCH})
message(STATUS "os arch ${OS_ARCH}") # x86_64 or aarch64


SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -static-libstdc++ ${DEBUG_SYMBOL} -pipe -Wall -fPIC -Wno-reorder ${ASAN_COMPILE_OPTION} -D__OMS_VERSION__=\\\"${GIT_VERSION}\\\" ${OBLOGMSG_MAPPING} ${OBCDC_NAME_VAR}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 ${DEBUG_SYMBOL} -pipe -Wall -fPIC ${ASAN_COMPILE_OPTION} -D__STDC_LIMIT_MACROS -D__OMS_VERSION__=\\\"${GIT_VERSION}\\\" ${OBLOGMSG_MAPPING} ${OBCDC_NAME_VAR}")
if(OS_ARCH STREQUAL "x86_64")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX,C>:-m64>)
endif()
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    SET(PLATFORM_SPEC rt)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    SET(PLATFORM_SPEC ${DEP_LIB}
            "-framework CoreFoundation"
            "-framework CoreGraphics"
            "-framework CoreData"
            "-framework CoreText"
            "-framework Security"
            "-framework Foundation"
            "-Wl,-U,_MallocExtension_ReleaseFreeMemory"
            "-Wl,-U,_ProfilerStart"
            "-Wl,-U,_ProfilerStop")
endif ()

########### start define deps #############################################################################################
set(DEP_INC
        ${JAVA_INCLUDE_DIR}
        ${OBLOGMSG_INCLUDE_DIR}
        ${LZ4_INCLUDE_DIR}
        ${JSONCPP_INCLUDE_DIR}
        ${LIBEVENT_INCLUDE_DIR}
        ${GFLAGS_INCLUDE_DIR}
        ${GLOG_INCLUDE_DIR}
        ${PROTOBUF_INCLUDE_DIR}
        ${GTEST_INCLUDE_DIR}
        ${OPENSSL_INCLUDE_DIR}
        )
set(DEP_LIB_PATH
        ${DEP_LIB_PATH}

        ${JAVA_LIB_DIR}
        ${OBLOGMSG_LIB_DIR}
        ${LIBOBLOG_LIB_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        )
set(DEP_LIBS
        ${OBLOGMSG_LIBRARIES}
        ${GTEST_LIBRARIES}
        ${GLOG_LIBRARIES}
        ${GFLAGS_LIBRARIES}
        ${PROTOBUF_LIBRARIES}
        ${PROTOBUF_PROTOC_LIBRARY}
        ${LIBEVENT_LIBRARIES}
        ${JSONCPP_LIBRARIES}
        ${LZ4_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        m
        ${PLATFORM_SPEC}
        ${CMAKE_THREAD_LIBS_INIT}
        )
message("DEP_INC: ${DEP_INC}")
message("DEP_LIB_PATH: ${DEP_LIB_PATH}")
message("DEP_LIBS: ${DEP_LIBS}")
########### end define deps ###############################################################################################

# common
set(COMMON_INC
        ${DEP_INC}
        ${CMAKE_CURRENT_BINARY_DIR}/proto
        ./src
        ./src/common/
        )
file(GLOB COMMON_SRC
        ./src/common/*.cpp
        ./src/common/*.hpp
        ./src/codec/*.cpp
        ./src/communication/*.cpp
        ./src/obaccess/*.cpp
        ./src/metric/*.cpp
        )
add_library(common STATIC ${COMMON_SRC} $<TARGET_OBJECTS:PROTO_OBJS>)
add_dependencies(common oblogmsg lz4 jsoncpp libevent protobuf)
if (WITH_GLOG)
    add_dependencies(common glog)
endif ()
target_include_directories(common PUBLIC ${COMMON_INC})

if (USE_OBCDC_NS)
    set(DEP_OBCDC_LIB obcdc ${LIBOBLOG_DEPS})
else ()
    set(DEP_OBCDC_LIB oblog ${LIBOBLOG_DEPS})
endif ()
# oblogreader
set(OBLOGREADER_INC ${COMMON_INC} ./src/oblogreader/ ${LIBOBLOG_INCLUDE_PATH})
file(GLOB OBLOGREADER_SRC ./src/oblogreader/*.cpp)
add_library(oblogreader_static STATIC ${OBLOGREADER_SRC})
add_dependencies(oblogreader_static common oblogmsg)
set_target_properties(oblogreader_static PROPERTIES OUTPUT_NAME "oblogreader")
target_include_directories(oblogreader_static PUBLIC ${OBLOGREADER_INC})
target_link_directories(oblogreader_static PUBLIC ${DEP_LIB_PATH})
target_link_libraries(oblogreader_static libcommon.a ${DEP_OBCDC_LIB} ${DEP_LIBS})

# oblogreader_jni
if (WITH_JNI_LIB)
    set(OBLOGREADER_JNI_INC ${OBLOGREADER_INC} src/jni)
    file(GLOB OBLOGREADER_JNI_SRC src/jni/*.cpp)
    add_library(oblogreader_jni STATIC ${OBLOGREADER_JNI_SRC})
    add_dependencies(oblogreader_jni oblogreader_static)
    target_include_directories(oblogreader_jni PUBLIC ${OBLOGREADER_JNI_INC})
    target_link_directories(oblogreader_jni PUBLIC ${DEP_LIB_PATH})
    target_link_libraries(oblogreader_jni liboblogreader.a libcommon.a ${DEP_OBCDC_LIB} ${DEP_LIBS})
endif ()

# logproxy static
set(LOGPROXY_INC ${OBLOGREADER_INC} src/arranger/)
file(GLOB LOGPROXY_SRC ./src/arranger/*.cpp)
message("SRC: ${LOGPROXY_SRC}")
add_library(logproxy_static STATIC ${LOGPROXY_SRC})
add_dependencies(logproxy_static oblogreader_static oblogmsg)
set_target_properties(logproxy_static PROPERTIES OUTPUT_NAME "logproxy")
target_include_directories(logproxy_static PUBLIC ${LOGPROXY_INC})
target_link_directories(logproxy_static PUBLIC ${DEP_LIB_PATH})
target_link_libraries(logproxy_static liboblogreader.a libcommon.a ${DEP_OBCDC_LIB} ${DEP_LIBS})

SET(BASE_LIBS liblogproxy.a liboblogreader.a libcommon.a)

# logproxy
add_executable(logproxy ./src/entry.cpp)
add_dependencies(logproxy logproxy_static)
target_include_directories(logproxy PUBLIC ${DEP_INC} ${LOGPROXY_INC})
target_link_directories(logproxy PUBLIC ${DEP_LIB_PATH})
target_link_libraries(logproxy ${BASE_LIBS} ${DEP_OBCDC_LIB} ${DEP_LIBS})
target_link_options(logproxy PUBLIC -static-libstdc++ ${ASAN_LINK_OPTION})

if (WITH_DEMO)
    # demo client
    file(GLOB DEMO_CLIENT_SRC ./src/demo/client_demo.cpp)
    add_executable(demo_client ${DEMO_CLIENT_SRC})
    add_dependencies(demo_client common)
    target_include_directories(demo_client PUBLIC ${COMMON_INC})
    target_link_directories(demo_client PUBLIC ${DEP_LIB_PATH})
    target_link_libraries(demo_client libcommon.a ${DEP_LIBS})
    target_link_options(demo_client PUBLIC -static-libstdc++ ${ASAN_LINK_OPTION})
endif ()

if (WITH_TEST)
    # test_base
    file(GLOB TEST_BASE_SRC ./src/test/test_entry.cpp)
    add_executable(test_base ${TEST_BASE_SRC})
    add_dependencies(test_base common oblogmsg gtest)
    target_include_directories(test_base PUBLIC ${LOGPROXY_INC})
    target_link_directories(test_base PUBLIC ${DEP_LIB_PATH})
    target_link_libraries(test_base libcommon.a ${DEP_LIBS})
    target_link_options(test_base PUBLIC -static-libstdc++ ${ASAN_LINK_OPTION})

    # test_oblogreader
    #    file(GLOB TEST_OBLOGREADER_SRC ./src/test/test_oblogreader.cpp)
    #    add_executable(test_oblogreader ${TEST_OBLOGREADER_SRC})
    #    add_dependencies(test_oblogreader logproxy_static gtest)
    #    target_include_directories(test_oblogreader PUBLIC ${LOGPROXY_INC})
    #    target_link_directories(test_oblogreader PUBLIC ${DEP_LIB_PATH})
    #    target_link_libraries(test_oblogreader ${BASE_LIBS} ${DEP_OBCDC_LIB} ${DEP_LIBS})
    #    target_link_options(test_oblogreader PUBLIC ${ASAN_LINK_OPTION})
endif ()

include(rpm)
