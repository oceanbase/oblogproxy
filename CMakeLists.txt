cmake_minimum_required(VERSION 3.2)
project(oblogproxy CXX)

option(WITH_DEBUG "With debug symbols" ON)
option(WITH_ASAN "Compile with AddressSanitizer" OFF)
option(WITH_TEST "With Tests" OFF)
option(WITH_DEMO "With Demos" OFF)
option(WITH_JNI_LIB "With oblogreader jni lib" OFF)
option(WITH_GLOG "With google log" ON)
option(WITH_DEPS "With precompiled deps" ON)
option(USE_LIBOBLOG "With precompiled liboblog" OFF)
option(USE_CXX11_ABI "Build with C++11 ABI" OFF)

SET(OMS_PROJECT_SRC_PATH ${CMAKE_CURRENT_SOURCE_DIR})
SET(OMS_PROJECT_BUILD_PATH ${CMAKE_CURRENT_BINARY_DIR})
SET(EXECUTABLE_OUTPUT_PATH ${OMS_PROJECT_BUILD_PATH})
SET(LIBRARY_OUTPUT_PATH ${OMS_PROJECT_BUILD_PATH})

SET(DEP_VAR $ENV{DEP_VAR})
SET(JAVA_HOME $ENV{JAVA_HOME})

SET(FIND_LIBOBLOG ON)
if (WITH_DEPS)
    execute_process(
            COMMAND bash deps/dep_create.sh ${OMS_PROJECT_BUILD_PATH}/deps
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            COMMAND_ERROR_IS_FATAL ANY
    )
    SET(DEP_VAR ${OMS_PROJECT_BUILD_PATH}/deps)

    # make liboblog found
    if (NOT USE_LIBOBLOG)
        SET(FIND_LIBOBLOG OFF)
        SET(LIBOBLOG_INCLUDE_PATH ${DEP_VAR}/usr/include/)
        SET(LIBOBLOG_LIBRARIES ${DEP_VAR}/usr/lib/liboblog.so)
    endif ()

    # make openssl found
    CMAKE_POLICY(SET CMP0074 NEW)
    SET(OpenSSL_ROOT ${DEP_VAR}/usr/local/oceanbase/deps/devel/)

    # make protoc found libstdc++
    SET(PROTOC_LINK_FLAGS "-L${DEP_VAR}/usr/local/oceanbase/deps/devel/lib/ -static-libstdc++")

    SET(DEP_LIB_PATH ${DEP_VAR}/usr/local/oceanbase/deps/devel/lib/ ${DEP_VAR}/usr/local/oceanbase/devtools/lib64/)
endif ()

message(STATUS "DEP_VAR: ${DEP_VAR}")
message(STATUS "JAVA_HOME: ${JAVA_HOME}")

# compiler
find_program(CC NAMES gcc PATHS ${DEP_VAR}/usr/local/oceanbase/devtools/bin/ ${DEP_VAR}/usr/local/gcc-5.2.0/bin/ /usr/bin/ NO_DEFAULT_PATH)
find_program(CXX NAMES g++ PATHS ${DEP_VAR}/usr/local/oceanbase/devtools/bin/ ${DEP_VAR}/usr/local/gcc-5.2.0/bin/ /usr/bin/ NO_DEFAULT_PATH)
find_program(AR NAMES gcc-ar ar PATHS ${DEP_VAR}/usr/local/oceanbase/devtools/bin/ ${DEP_VAR}/usr/local/gcc-5.2.0/bin/ /usr/bin/ NO_DEFAULT_PATH)
SET(CMAKE_C_COMPILER ${CC})
SET(CMAKE_CXX_COMPILER ${CXX})
SET(CMAKE_C_COMPILER_AR ${AR})
SET(CMAKE_CXX_COMPILER_AR ${AR})
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "AR compiler: ${CMAKE_C_COMPILER_AR}")

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

# oblogmsg
include(oblogmsg)
SET(OBLOGMSG_MAPPING "")

# oblog
if (FIND_LIBOBLOG)
    find_path(LIBOBLOG_INCLUDE_PATH NAMES liboblog.h)
    find_library(LIBOBLOG_LIBRARIES NAMES liboblog.so)
    if ((NOT LIBOBLOG_INCLUDE_PATH) OR (NOT LIBOBLOG_LIBRARIES))
        message(FATAL_ERROR "Fail to find liboblog")
    endif ()
endif ()
GET_FILENAME_COMPONENT(LIBOBLOG_LIB_DIR ${LIBOBLOG_LIBRARIES} DIRECTORY)

message("oblogmsg: ${OBLOGMSG_INCLUDE_DIR}, ${OBLOGMSG_LIBRARIES}")
message("liboblog: ${LIBOBLOG_INCLUDE_PATH}, ${LIBOBLOG_LIBRARIES}")

execute_process(
        COMMAND git log -1 --format=%H
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

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

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -static-libstdc++ ${DEBUG_SYMBOL} -pipe -m64 -Wall -fPIC -Wno-reorder ${ASAN_COMPILE_OPTION} -D__OMS_VERSION__=\\\"${GIT_VERSION}\\\" ${OBLOGMSG_MAPPING}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 ${DEBUG_SYMBOL} -pipe -m64 -Wall -fPIC ${ASAN_COMPILE_OPTION} -D__STDC_LIMIT_MACROS -D__OMS_VERSION__=\\\"${GIT_VERSION}\\\" ${OBLOGMSG_MAPPING}")


if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    SET(PLATFORM_SPEC rt)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc")
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
        )
add_library(common STATIC ${COMMON_SRC} $<TARGET_OBJECTS:PROTO_OBJS>)
add_dependencies(common oblogmsg lz4 jsoncpp libevent protobuf)
if (WITH_GLOG)
    add_dependencies(common glog)
endif ()
target_include_directories(common PUBLIC ${COMMON_INC})

# oblogreader
set(OBLOGREADER_INC ${COMMON_INC} ./src/oblogreader/ ${LIBOBLOG_INCLUDE_PATH})
file(GLOB OBLOGREADER_SRC ./src/oblogreader/*.cpp)
add_library(oblogreader_static STATIC ${OBLOGREADER_SRC})
add_dependencies(oblogreader_static common oblogmsg)
set_target_properties(oblogreader_static PROPERTIES OUTPUT_NAME "oblogreader")
target_include_directories(oblogreader_static PUBLIC ${OBLOGREADER_INC})
target_link_directories(oblogreader_static PUBLIC ${DEP_LIB_PATH})
target_link_libraries(oblogreader_static libcommon.a oblog ${DEP_LIBS})

# oblogreader_jni
if (WITH_JNI_LIB)
    set(OBLOGREADER_JNI_INC ${OBLOGREADER_INC} src/jni)
    file(GLOB OBLOGREADER_JNI_SRC src/jni/*.cpp)
    add_library(oblogreader_jni STATIC ${OBLOGREADER_JNI_SRC})
    add_dependencies(oblogreader_jni oblogreader_static)
    target_include_directories(oblogreader_jni PUBLIC ${OBLOGREADER_JNI_INC})
    target_link_directories(oblogreader_jni PUBLIC ${DEP_LIB_PATH})
    target_link_libraries(oblogreader_jni liboblogreader.a libcommon.a oblog ${DEP_LIBS})
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
target_link_libraries(logproxy_static liboblogreader.a libcommon.a oblog ${DEP_LIBS})

SET(BASE_LIBS liblogproxy.a liboblogreader.a libcommon.a)

# logproxy
add_executable(logproxy ./src/entry.cpp)
add_dependencies(logproxy logproxy_static)
target_include_directories(logproxy PUBLIC ${DEP_INC} ${LOGPROXY_INC})
target_link_directories(logproxy PUBLIC ${DEP_LIB_PATH})
target_link_libraries(logproxy ${BASE_LIBS} oblog ${DEP_LIBS})
target_link_options(logproxy PUBLIC "${ASAN_LINK_OPTION}")

if (WITH_DEMO)
    # demo client
    file(GLOB DEMO_CLIENT_SRC ./src/demo/client_demo.cpp)
    add_executable(demo_client ${DEMO_CLIENT_SRC})
    add_dependencies(demo_client common)
    target_include_directories(demo_client PUBLIC ${COMMON_INC})
    target_link_directories(demo_client PUBLIC ${DEP_LIB_PATH})
    target_link_libraries(demo_client libcommon.a ${DEP_LIBS})
    target_link_libraries(demo_client libcommon.a ${DEP_LIBS})
    target_link_options(demo_client PUBLIC ${ASAN_LINK_OPTION})
endif ()

if (WITH_TEST)
    # test_base
    file(GLOB TEST_BASE_SRC ./src/test/test_entry.cpp)
    add_executable(test_base ${TEST_BASE_SRC})
    add_dependencies(test_base logproxy_static gtest)
    target_include_directories(test_base PUBLIC ${LOGPROXY_INC})
    target_link_directories(test_base PUBLIC ${DEP_LIB_PATH})
    target_link_libraries(test_base ${BASE_LIBS} ${DEP_LIBS})
    target_link_options(test_base PUBLIC ${ASAN_LINK_OPTION})

    # test_oblogreader
    file(GLOB TEST_OBLOGREADER_SRC ./src/test/test_oblogreader.cpp)
    add_executable(test_oblogreader ${TEST_OBLOGREADER_SRC})
    add_dependencies(test_oblogreader logproxy_static gtest)
    target_include_directories(test_oblogreader PUBLIC ${LOGPROXY_INC})
    target_link_directories(test_oblogreader PUBLIC ${DEP_LIB_PATH})
    target_link_libraries(test_oblogreader ${BASE_LIBS} oblog ${DEP_LIBS})
    target_link_options(test_oblogreader PUBLIC ${ASAN_LINK_OPTION})
endif ()

include(rpm)
