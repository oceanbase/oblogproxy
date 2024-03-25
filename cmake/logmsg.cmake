# oblogmsg
add_library(oblogmsg SHARED IMPORTED GLOBAL)
set_target_properties(oblogmsg PROPERTIES IMPORTED_LOCATION ${DEP_VAR}/usr/local/oceanbase/deps/devel/lib/liboblogmsg.a)
target_include_directories(oblogmsg INTERFACE ${DEP_VAR}/usr/local/oceanbase/deps/devel/include/oblogmsg)
# logmsg
add_library(logmsg INTERFACE)
target_include_directories(logmsg INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/logmsg)
target_link_libraries(logmsg INTERFACE oblogmsg)
get_target_property(OBLOGMSG_INCLUDE_PATH oblogmsg INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(OBLOGMSG_LOCATION oblogmsg IMPORTED_LOCATION)
message(STATUS "OBLOGMSG_INCLUDE_PATH: ${OBLOGMSG_INCLUDE_PATH}, OBLOGMSG_LOCATION: ${OBLOGMSG_LOCATION}")