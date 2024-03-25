set(CPACK_GENERATOR "RPM")
set(CPACK_RPM_COMPONENT_INSTALL OFF)
# let rpmbuild determine rpm filename
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
set(CPACK_RPM_PACKAGE_RELEASE_DIST OFF)
# RPM package informations.
set(CPACK_PACKAGING_INSTALL_PREFIX /usr/local/oblogproxy)
list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/usr/local/oblogproxy")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "oblogproxy is a clog proxy server for OceanBase")
set(CPACK_PACKAGE_VENDOR "OceanBase Inc.")
set(CPACK_PACKAGE_VERSION "${OBLOGPROXY_VERSION}")
set(CPACK_RPM_PACKAGE_RELEASE "${OBLOGPROXY_RELEASEID}")
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
set(CPACK_PACKAGE_VERSION_MAJOR "${OBLOGPROXY_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${OBLOGPROXY_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${OBLOGPROXY_VERSION_PATCH}")
set(CPACK_SOURCE_IGNORE_FILES "${PROJECT_BINARY_DIR};/.git/;.gitignore")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Databases")
set(CPACK_RPM_PACKAGE_URL "https://open.oceanbase.com")
set(CPACK_RPM_PACKAGE_DESCRIPTION "oblogproxy is a clog proxy server for OceanBase")
set(CPACK_RPM_PACKAGE_LICENSE "Mulan PubL v2.")
set(CPACK_RPM_DEFAULT_USER "root")
set(CPACK_RPM_DEFAULT_GROUP "root")
set(CPACK_RPM_SPEC_MORE_DEFINE
        "%global _missing_build_ids_terminate_build 0
%global _find_debuginfo_opts -g
%define __debug_install_post %{_rpmconfigdir}/find-debuginfo.sh %{?_find_debuginfo_opts} %{_builddir}/%{?buildsubdir};%{nil}
%define debug_package %{nil}")

## TIPS
#
# - PATH is relative to the **ROOT directory** of project other than the cmake directory.
if (NOT ${OBLOGPROXY_INSTALL_PREFIX})
    set(CPACK_PACKAGING_INSTALL_PREFIX ${OBLOGPROXY_INSTALL_PREFIX})
endif ()

message("CPACK_PACKAGING_INSTALL_PREFIX: ${CPACK_PACKAGING_INSTALL_PREFIX}")

list(APPEND OBLOGPROXY_BIN_FILES ${CMAKE_BINARY_DIR}/logproxy)
list(APPEND OBLOGPROXY_BIN_FILES ${CMAKE_BINARY_DIR}/oblogreader)
list(APPEND OBLOGPROXY_BIN_FILES ${CMAKE_BINARY_DIR}/binlog_converter)
list(APPEND OBLOGPROXY_BIN_FILES ${CMAKE_SOURCE_DIR}/script/list_logreader_path.sh)
list(APPEND OBLOGPROXY_BIN_FILES ${CMAKE_SOURCE_DIR}/script/list_logreader_process.sh)
list(APPEND OBLOGPROXY_CONF_FILES ${CMAKE_SOURCE_DIR}/conf/conf.json)
list(APPEND OBLOGPROXY_SCRIPT_FILES ${CMAKE_SOURCE_DIR}/script/run.sh)

install(PROGRAMS
        ${OBLOGPROXY_BIN_FILES}
        DESTINATION ${CPACK_PACKAGING_INSTALL_PREFIX}/bin
        )

install(FILES
        ${OBLOGPROXY_CONF_FILES}
        DESTINATION ${CPACK_PACKAGING_INSTALL_PREFIX}/conf
        )

install(PROGRAMS
        ${OBLOGPROXY_SCRIPT_FILES}
        DESTINATION ${CPACK_PACKAGING_INSTALL_PREFIX}/
        )

function(install_library_target LIBRARY_TARGET INSTALL_DIR)
    get_target_property(LIB_IMPORT_LOCATION ${LIBRARY_TARGET} IMPORTED_LOCATION)
    install(FILES ${LIB_IMPORT_LOCATION}
            DESTINATION ${INSTALL_DIR}
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )
    get_target_property(LIB_IMPORT_SONAME ${LIBRARY_TARGET} IMPORTED_SONAME)
    if (LIB_IMPORT_SONAME)
        get_filename_component(LIB_DIR ${LIB_IMPORT_LOCATION} DIRECTORY)
        install(FILES ${LIB_DIR}/${LIB_IMPORT_SONAME}
                DESTINATION ${INSTALL_DIR}
                PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
                )
    endif ()
endfunction()

function(install_obcdc_target OBCDC_TARGET LIBRARY_TARGET INSTALL_DIR)
    install(TARGETS ${OBCDC_TARGET}
            LIBRARY DESTINATION ${INSTALL_DIR}
            ARCHIVE DESTINATION ${INSTALL_DIR}
            )

    install_library_target(${LIBRARY_TARGET} ${INSTALL_DIR})
    install_library_target(libaio ${INSTALL_DIR})
    set_target_properties(${OBCDC_TARGET} PROPERTIES INSTALL_RPATH "$ORIGIN")
endfunction()

#  install obcdc ce 3.x
message(STATUS "package/install with oceanbase ce cdc 3.x")
install_obcdc_target(obcdc-ce-3 libobcdcce3 ${CPACK_PACKAGING_INSTALL_PREFIX}/obcdc/obcdc-ce-3.x-access)

#  install obcdc ce 4.x
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

foreach (version IN LISTS version_list)
    message(STATUS "package/install with oceanbase ce cdc ${version}")
    install_obcdc_target(obcdc-ce-${version} libobcdcce${version} ${CPACK_PACKAGING_INSTALL_PREFIX}/obcdc/obcdc-ce-${version}.x-access)
    install_library_target(libmariadb ${CPACK_PACKAGING_INSTALL_PREFIX}/obcdc/obcdc-ce-${version}.x-access)
endforeach ()

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/utils_post.script "/sbin/ldconfig /usr/lib")
set(CPACK_RPM_UTILS_POST_INSTALL_SCRIPT_FILE ${CMAKE_CURRENT_BINARY_DIR}/utils_post.script)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/utils_postun.script "/sbin/ldconfig")
set(CPACK_RPM_UTILS_POST_UNINSTALL_SCRIPT_FILE ${CMAKE_CURRENT_BINARY_DIR}/utils_postun.script)

# install cpack to make everything work
include(CPack)

#add rpm target to create RPMS
add_custom_target(rpm
        COMMAND +make package
)