file(GLOB_RECURSE srcs CONFIGURE_DEPENDS ./*.cpp)

add_library(tool STATIC ${srcs})

target_link_libraries(tool
        PUBLIC antlr4
        PUBLIC obsqlparser
)

target_include_directories(tool
        PUBLIC ${ANTLR4_GENERATED_SRC_DIR}/oceanbase
        PUBLIC ${PROJECT_SOURCE_DIR}/include
        PUBLIC ${PROJECT_SOURCE_DIR}/src
)

add_dependencies(tool antlr4 obsqlparser)

set(ETRANSFER_LIB "${CMAKE_CURRENT_BINARY_DIR}/libetransfer.a")

set(TMP_DIR "${CMAKE_BINARY_DIR}/tmp")
file(MAKE_DIRECTORY ${TMP_DIR})

add_custom_command(
        OUTPUT ${ETRANSFER_LIB}
        COMMAND echo "Combining libraries..."

        COMMAND ${CMAKE_AR} -x $<TARGET_FILE:obsqlparser>
        COMMAND ${CMAKE_AR} -x $<TARGET_FILE:antlr4>
        COMMAND ${CMAKE_AR} -x $<TARGET_FILE:tool>

        COMMAND ${CMAKE_AR} -qc ${ETRANSFER_LIB} *.o

        COMMAND ${CMAKE_RANLIB} ${ETRANSFER_LIB}

        COMMAND echo "Libraries combined into ${ETRANSFER_LIB}"
        WORKING_DIRECTORY ${TMP_DIR}
        DEPENDS obsqlparser antlr4 tool
        COMMENT "Creating Combined Static Library"
)

add_custom_target(etransfer ALL
        DEPENDS ${ETRANSFER_LIB}
        COMMENT "Custom target to combine libraries"
)

add_dependencies(etransfer obsqlparser antlr4 tool)

install(FILES
        ${ETRANSFER_LIB}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

file(GLOB INCLUDE_CONVERT_FILES "${CMAKE_SOURCE_DIR}/include/convert/*.h")

install(FILES
        ${INCLUDE_CONVERT_FILES}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/convert
)
