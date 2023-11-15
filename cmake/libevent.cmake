include(ExternalProject)

set(MODULE_NAME libevent)
set(LIBEVENT_SOURCES_DIR ${THIRD_PARTY_PATH}/${MODULE_NAME})
set(LIBEVENT_INSTALL_DIR ${THIRD_PARTY_PATH}/install/${MODULE_NAME})

set(LIBEVENT_INCLUDE_DIR "${LIBEVENT_INSTALL_DIR}/include")

set(prefix_path "${LIBEVENT_INSTALL_DIR}")

ExternalProject_Add(
        extern_libevent
        ${EXTERNAL_PROJECT_LOG_ARGS}
        GIT_REPOSITORY "https://github.com/libevent/libevent"
        GIT_TAG "release-2.1.12-stable"
        PREFIX ${LIBEVENT_SOURCES_DIR}
        UPDATE_COMMAND ""
        CMAKE_ARGS
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_INSTALL_PREFIX=${LIBEVENT_INSTALL_DIR}
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DEVENT__DISABLE_OPENSSL=ON
        -DEVENT__DISABLE_TESTS=ON
        -DEVENT__DISABLE_DEBUG_MODE=ON
        -DEVENT__DISABLE_BENCHMARK=ON
        -DEVENT__DISABLE_REGRESS=ON
        -DEVENT__DISABLE_SAMPLES=ON
        -DEVENT__FORCE_KQUEUE_CHECK=ON
        -DEVENT__LIBRARY_TYPE=STATIC
        -DCMAKE_BUILD_TYPE=${THIRD_PARTY_BUILD_TYPE}
)

#======== added equivalent targets to those provided by libevent2 itself =========================>
## fix issue: Imported target "libevent::xxxx" includes non-existent path
if (NOT EXISTS ${LIBEVENT_INCLUDE_DIR})
    execute_process(COMMAND mkdir -p ${LIBEVENT_INCLUDE_DIR} COMMAND_ERROR_IS_FATAL ANY)
endif ()

add_library(${MODULE_NAME}::core STATIC IMPORTED GLOBAL)
add_dependencies(${MODULE_NAME}::core extern_libevent)
set_target_properties(${MODULE_NAME}::core PROPERTIES IMPORTED_LOCATION "${LIBEVENT_INSTALL_DIR}/lib/libevent_core.a")
target_include_directories(${MODULE_NAME}::core INTERFACE ${LIBEVENT_INCLUDE_DIR})
target_link_libraries(${MODULE_NAME}::core INTERFACE pthread)

add_library(${MODULE_NAME}::extra STATIC IMPORTED GLOBAL)
add_dependencies(${MODULE_NAME}::extra extern_libevent)
set_target_properties(${MODULE_NAME}::extra PROPERTIES IMPORTED_LOCATION "${LIBEVENT_INSTALL_DIR}/lib/libevent_extra.a")
target_include_directories(${MODULE_NAME}::extra INTERFACE ${LIBEVENT_INCLUDE_DIR})
target_link_libraries(${MODULE_NAME}::extra INTERFACE pthread ${MODULE_NAME}::core)

add_library(${MODULE_NAME}::pthreads STATIC IMPORTED GLOBAL)
add_dependencies(${MODULE_NAME}::pthreads extern_libevent)
set_target_properties(${MODULE_NAME}::pthreads PROPERTIES IMPORTED_LOCATION "${LIBEVENT_INSTALL_DIR}/lib/libevent_pthreads.a")
target_include_directories(${MODULE_NAME}::pthreads INTERFACE ${LIBEVENT_INCLUDE_DIR})
target_link_libraries(${MODULE_NAME}::pthreads INTERFACE pthread ${MODULE_NAME}::core)
#<======= added equivalent targets to those provided by libevent2 itself ==========================
