cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(etransfer-cpp LANGUAGES CXX)

option(WITH_TEST "Compile etransfer-cpp test executables" ON)
option(WITH_EXAMPLE "Compile etransfer-cpp example executables" ON)
option(WITH_ASAN "build with ASAN" OFF)

if (WITH_ASAN)
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libasan") 
endif()

set(THIRD_PARTY_PATH ${CMAKE_BINARY_DIR}/thirdparty)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(antlr4)
include(generate)

add_subdirectory(src)

if (WITH_TEST)
    add_subdirectory(test)
endif ()

if (WITH_EXAMPLE)
    add_subdirectory(example)
endif ()