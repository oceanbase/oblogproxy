include(ExternalProject)
# Always invoke `FIND_PACKAGE(Protobuf)` for importing function protobuf_generate_cpp

find_package(Protobuf QUIET)
macro(UNSET_VAR VAR_NAME)
    unset(${VAR_NAME} CACHE)
    unset(${VAR_NAME})
endmacro()

unset_var(PROTOBUF_FOUND)
unset_var(PROTOBUF_PROTOC_EXECUTABLE)
unset_var(PROTOBUF_PROTOC_LIBRARY)
unset_var(PROTOBUF_LITE_LIBRARY)
unset_var(PROTOBUF_LIBRARY)
unset_var(PROTOBUF_INCLUDE_DIR)
unset_var(Protobuf_PROTOC_EXECUTABLE)

if (POLICY CMP0097)
    cmake_policy(SET CMP0097 NEW)
endif ()

# Print and set the protobuf library information,
# finish this cmake process and exit from this file.
macro(prompt_protobuf_lib)
    set(protobuf_DEPS ${ARGN})

    message(STATUS "Protobuf protoc executable: ${PROTOBUF_PROTOC_EXECUTABLE}")
    message(STATUS "Protobuf-lite library: ${PROTOBUF_LITE_LIBRARY}")
    message(STATUS "Protobuf library: ${PROTOBUF_LIBRARY}")
    message(STATUS "Protoc library: ${PROTOBUF_PROTOC_LIBRARY}")
    message(STATUS "Protobuf version: ${PROTOBUF_VERSION}")

    # Assuming that all the protobuf libraries are of the same type.
    if (${PROTOBUF_LIBRARY} MATCHES ${CMAKE_STATIC_LIBRARY_SUFFIX})
        set(protobuf_LIBTYPE STATIC)
    elseif (${PROTOBUF_LIBRARY} MATCHES "${CMAKE_SHARED_LIBRARY_SUFFIX}$")
        set(protobuf_LIBTYPE SHARED)
    else ()
        message(FATAL_ERROR "Unknown library type: ${PROTOBUF_LIBRARY}")
    endif ()

    if (NOT EXISTS ${PROTOBUF_INCLUDE_DIR})
        execute_process(COMMAND mkdir -p ${PROTOBUF_INCLUDE_DIR} COMMAND_ERROR_IS_FATAL ANY)
    endif ()

    add_library(protobuf ${protobuf_LIBTYPE} IMPORTED GLOBAL)
    set_target_properties(protobuf PROPERTIES IMPORTED_LOCATION ${PROTOBUF_LIBRARY})
    target_include_directories(protobuf INTERFACE ${PROTOBUF_INCLUDE_DIR})

    add_library(protobuf_lite ${protobuf_LIBTYPE} IMPORTED GLOBAL)
    set_target_properties(protobuf_lite PROPERTIES IMPORTED_LOCATION ${PROTOBUF_LITE_LIBRARY})
    target_include_directories(protobuf_lite INTERFACE ${PROTOBUF_INCLUDE_DIR})

    add_library(libprotoc ${protobuf_LIBTYPE} IMPORTED GLOBAL)
    set_target_properties(libprotoc PROPERTIES IMPORTED_LOCATION ${PROTOBUF_PROTOC_LIBRARY})
    target_include_directories(libprotoc INTERFACE ${PROTOBUF_INCLUDE_DIR})

    add_executable(protoc IMPORTED GLOBAL)
    set_target_properties(protoc PROPERTIES IMPORTED_LOCATION ${PROTOBUF_PROTOC_EXECUTABLE})

    foreach (dep ${protobuf_DEPS})
        add_dependencies(protobuf ${dep})
        add_dependencies(protobuf_lite ${dep})
        add_dependencies(libprotoc ${dep})
        add_dependencies(protoc ${dep})
    endforeach ()

    return()
endmacro()

function(build_protobuf TARGET_NAME)
    string(REPLACE "extern_" "" TARGET_DIR_NAME "${TARGET_NAME}")
    set(PROTOBUF_SOURCES_DIR ${THIRD_PARTY_PATH}/${TARGET_DIR_NAME})
    set(PROTOBUF_INSTALL_DIR ${THIRD_PARTY_PATH}/install/${TARGET_DIR_NAME})

    set(${TARGET_NAME}_INCLUDE_DIR "${PROTOBUF_INSTALL_DIR}/include" PARENT_SCOPE)
    set(${TARGET_NAME}_LITE_LIBRARY
            "${PROTOBUF_INSTALL_DIR}/lib64/libprotobuf-lite${CMAKE_STATIC_LIBRARY_SUFFIX}"
            PARENT_SCOPE)
    set(${TARGET_NAME}_LIBRARY
            "${PROTOBUF_INSTALL_DIR}/lib64/libprotobuf${CMAKE_STATIC_LIBRARY_SUFFIX}"
            PARENT_SCOPE)
    set(${TARGET_NAME}_PROTOC_LIBRARY
            "${PROTOBUF_INSTALL_DIR}/lib64/libprotoc${CMAKE_STATIC_LIBRARY_SUFFIX}"
            PARENT_SCOPE)
    set(${TARGET_NAME}_PROTOC_EXECUTABLE
            "${PROTOBUF_INSTALL_DIR}/bin/protoc${CMAKE_EXECUTABLE_SUFFIX}"
            PARENT_SCOPE)

    set(OPTIONAL_CACHE_ARGS "")
    set(OPTIONAL_ARGS
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DCMAKE_EXE_LINKER_FLAGS=${PROTOC_LINK_FLAGS}"
            "-Dprotobuf_WITH_ZLIB=OFF")

    set(PROTOBUF_REPO "https://github.com/protocolbuffers/protobuf.git")
    set(PROTOBUF_TAG "v3.6.0")

    ExternalProject_Add(
            ${TARGET_NAME}
            ${EXTERNAL_PROJECT_LOG_ARGS}
            PREFIX ${PROTOBUF_SOURCES_DIR}
            UPDATE_COMMAND ""
            GIT_REPOSITORY ${PROTOBUF_REPO}
            GIT_TAG ${PROTOBUF_TAG}
            GIT_SUBMODULES ""
            GIT_SUBMODULES_RECURSE "false"
            CONFIGURE_COMMAND
            ${CMAKE_COMMAND} ${PROTOBUF_SOURCES_DIR}/src/${TARGET_NAME}/cmake
            ${OPTIONAL_ARGS}
            -Dprotobuf_BUILD_TESTS=OFF
            -DCMAKE_SKIP_RPATH=ON
            -Dprotobuf_VERBOSE=ON
            -DCMAKE_POSITION_INDEPENDENT_CODE=ON
            -DCMAKE_BUILD_TYPE=${THIRD_PARTY_BUILD_TYPE}
            -DCMAKE_INSTALL_PREFIX=${PROTOBUF_INSTALL_DIR}
            -DBUILD_SHARED_LIBS=OFF
            -Dprotobuf_MSVC_STATIC_RUNTIME=${MSVC_STATIC_CRT}
            CMAKE_CACHE_ARGS
            -DCMAKE_INSTALL_PREFIX:PATH=${PROTOBUF_INSTALL_DIR}
            -DCMAKE_BUILD_TYPE:STRING=${THIRD_PARTY_BUILD_TYPE}
            -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF
            ${OPTIONAL_CACHE_ARGS}
    )
ENDFUNCTION()

build_protobuf(extern_protobuf FALSE)

set(PROTOBUF_INCLUDE_DIR ${extern_protobuf_INCLUDE_DIR})
set(PROTOBUF_LITE_LIBRARY ${extern_protobuf_LITE_LIBRARY})
set(PROTOBUF_LIBRARY ${extern_protobuf_LIBRARY})
set(PROTOBUF_PROTOC_LIBRARY ${extern_protobuf_PROTOC_LIBRARY})

set(PROTOBUF_PROTOC_EXECUTABLE ${extern_protobuf_PROTOC_EXECUTABLE})
set(Protobuf_PROTOC_EXECUTABLE ${extern_protobuf_PROTOC_EXECUTABLE})

set(PROTOBUF_VERSION 3.6.0)

prompt_protobuf_lib(extern_protobuf)