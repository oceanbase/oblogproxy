######### get arch ###########################################
execute_process(COMMAND uname -m
        OUTPUT_VARIABLE ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
        )
set(OS_ARCH ${ARCH})
message(STATUS "os arch ${OS_ARCH}") # x86_64 or aarch64

if (WITH_DEPS)
    # download
    execute_process(
            COMMAND bash deps/dep_create.sh tool ${DEP_VAR} obdevtools-gcc9
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMAND_ERROR_IS_FATAL ANY)
    set(COMPILER_DIR ${DEP_VAR}/usr/local/oceanbase/devtools/bin/)
    set(GCC_LIB_DIR ${DEP_VAR}/usr/local/oceanbase/devtools/lib/gcc/x86_64-redhat-linux/9)
else ()
    # find in current system
    execute_process(
            COMMAND which gcc
            OUTPUT_VARIABLE GCC_BIN
    )
    get_filename_component(COMPILER_DIR ${GCC_BIN} DIRECTORY)
endif ()

message(STATUS "COMPILER_DIR: ${COMPILER_DIR}")

find_program(CC NAMES gcc PATHS ${COMPILER_DIR} NO_DEFAULT_PATH)
find_program(CXX NAMES g++ PATHS ${COMPILER_DIR} NO_DEFAULT_PATH)
find_program(AR NAMES gcc-ar ar PATHS ${COMPILER_DIR} NO_DEFAULT_PATH)
find_program(RANLIB NAMES gcc-ranlib ranlib PATHS ${COMPILER_DIR} NO_DEFAULT_PATH)
set(CMAKE_C_COMPILER ${CC})
set(CMAKE_CXX_COMPILER ${CXX})
set(CMAKE_AR ${AR})
set(CMAKE_RANLIB ${RANLIB})
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "ar: ${CMAKE_AR}")
message(STATUS "ranlib: ${CMAKE_RANLIB}")

get_filename_component(COMPILER_DIR ${CMAKE_C_COMPILER} DIRECTORY)
get_filename_component(COMPILER_BASE_DIR ${COMPILER_DIR} DIRECTORY)
set(CXX_LIB_DIR ${COMPILER_BASE_DIR}/lib64/)
message(STATUS "CXX_LIB_DIR: ${CXX_LIB_DIR}, GCC_LIB_DIR: ${GCC_LIB_DIR}")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif ()
message(STATUS "build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS 0)
set(CMAKE_POSITION_INDEPENDENT_CODE 1)

## ensure that the build results can be run on systems with lower libstdc++ version than the build system
add_compile_definitions($<$<COMPILE_LANGUAGE:CXX>:_GLIBCXX_USE_CXX11_ABI=0>)
link_directories(${CXX_LIB_DIR} ${GCC_LIB_DIR})
add_link_options($<$<COMPILE_LANGUAGE:CXX>:-static-libstdc++>)
add_link_options($<$<COMPILE_LANGUAGE:CXX,C>:-static-libgcc>)

if (WITH_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif ()

if (WITH_DEBUG)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-ggdb>)
else ()
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-ggdb>)
    add_compile_definitions($<$<COMPILE_LANGUAGE:CXX>:NDEBUG>)
endif ()

add_compile_definitions($<$<COMPILE_LANGUAGE:C>:__STDC_LIMIT_MACROS>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX,C>:-Wall>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX,C>:-Werror>)
## Organize the three-party compilation, and then remove the ignored item
add_compile_options($<$<COMPILE_LANGUAGE:CXX,C>:-Wno-sign-compare>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX,C>:-Wno-class-memaccess>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX,C>:-Wno-reorder>)
if (OS_ARCH STREQUAL "x86_64")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX,C>:-m64>)
endif ()
add_compile_options($<$<COMPILE_LANGUAGE:CXX,C>:-pipe>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX,C>:-fPIC>)

add_compile_options($<$<COMPILE_LANGUAGE:CXX,C>:-pie>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX,C>:-znoexecstack>)
add_compile_options($<$<COMPILE_LANGUAGE:CXX,C>:-znow>)

add_link_options(-lm)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_link_options(-lrt)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_link_options($<$<COMPILE_LANGUAGE:CXX,C>:
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

include(ProcessorCount)
message(STATUS "NUM_OF_PROCESSOR: ${NUM_OF_PROCESSOR}")
ProcessorCount(NUM_OF_PROCESSOR)