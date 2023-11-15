set(CPACK_GENERATOR "RPM")
# use seperated RPM SPECs and generate different RPMs
set(CPACK_COMPONENTS_IGNORE_GROUPS 1)
set(CPACK_RPM_COMPONENT_INSTALL ON)
# use "oblogproxy" as main component so its RPM filename won't have "oblogproxy"
set(CPACK_RPM_MAIN_COMPONENT "oblogproxy")
# let rpmbuild determine rpm filename
set(CPACK_RPM_FILE_NAME "RPM-DEFAULT")
set(CPACK_RPM_PACKAGE_RELEASE ${OBLOGPROXY_RELEASEID})
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)
# RPM package informations.
set(CPACK_PACKAGING_INSTALL_PREFIX /usr/local/oblogproxy)
list(APPEND CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION "/usr/local/oblogproxy")
set(CPACK_PACKAGE_NAME ${OBLOGPROXY_PACKAGE_NAME})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "oblogproxy is a clog proxy server for OceanBase CE")
set(CPACK_PACKAGE_VENDOR "Ant Group CO., Ltd.")
set(CPACK_PACKAGE_VERSION ${OBLOGPROXY_PACKAGE_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR 1)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 1)
set(CPACK_RPM_PACKAGE_GROUP "Applications/Databases")
set(CPACK_RPM_PACKAGE_URL "https://open.oceanbase.com")
set(CPACK_RPM_PACKAGE_DESCRIPTION "oblogproxy is a clog proxy server for OceanBase CE")
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
endif()

message("CPACK_PACKAGING_INSTALL_PREFIX: ${CPACK_PACKAGING_INSTALL_PREFIX}")

list(APPEND OBLOGPROXY_BIN_FILES ${OMS_PROJECT_BUILD_PATH}/logproxy)
list(APPEND OBLOGPROXY_CONF_FILES ${CMAKE_SOURCE_DIR}/conf/conf.json)
list(APPEND OBLOGPROXY_SCRIPT_FILES ${CMAKE_SOURCE_DIR}/script/run.sh)

install(PROGRAMS
        ${OBLOGPROXY_BIN_FILES}
        DESTINATION ${CPACK_PACKAGING_INSTALL_PREFIX}/bin
        COMPONENT oblogproxy
        )

install(FILES
        ${OBLOGPROXY_CONF_FILES}
        DESTINATION ${CPACK_PACKAGING_INSTALL_PREFIX}/conf
        COMPONENT oblogproxy
        )

install(PROGRAMS
        ${OBLOGPROXY_SCRIPT_FILES}
        DESTINATION ${CPACK_PACKAGING_INSTALL_PREFIX}/
        COMPONENT oblogproxy
        )

if (WITH_DEPS)
    if (NOT USE_LIBOBLOG)
        if (USE_LIBOBLOG_3)
            file(GLOB LIBOBLOG_OUTPUT_LIBS
                    ${DEP_VAR}/usr/lib/${OBCDC_NAME}*.so*
                    ${DEP_VAR}/usr/local/oceanbase/deps/devel/lib/libaio.so*
                    )
            install(FILES
                    ${LIBOBLOG_OUTPUT_LIBS}
                    DESTINATION ${CPACK_PACKAGING_INSTALL_PREFIX}/liboblog
                    COMPONENT oblogproxy
                    )
        else()
            file(GLOB LIBOBLOG_OUTPUT_LIBS ${DEP_VAR}/home/admin/oceanbase/lib64/${OBCDC_NAME}*.so*
                    ${DEP_VAR}/usr/local/oceanbase/deps/devel/lib/libaio.so*
                    ${DEP_VAR}/usr/local/oceanbase/deps/devel/lib/mariadb/libmariadb.so*
                    )
            install(FILES
                    ${LIBOBLOG_OUTPUT_LIBS}
                    DESTINATION ${CPACK_PACKAGING_INSTALL_PREFIX}/liboblog
                    COMPONENT oblogproxy
                    )
        endif()
    endif()
endif()

file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/utils_post.script "/sbin/ldconfig /usr/lib")
set(CPACK_RPM_UTILS_POST_INSTALL_SCRIPT_FILE  ${CMAKE_CURRENT_BINARY_DIR}/utils_post.script)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/utils_postun.script "/sbin/ldconfig")
set(CPACK_RPM_UTILS_POST_UNINSTALL_SCRIPT_FILE  ${CMAKE_CURRENT_BINARY_DIR}/utils_postun.script)
if(USE_OBCDC_NS)
    if (USE_LIBOBLOG_3)
        set(CPACK_RPM_PACKAGE_REQUIRES "devdeps-libaio >= 0.3.112")
    else()
        set(CPACK_RPM_PACKAGE_REQUIRES "devdeps-libaio >= 0.3.112")
    endif()
else()
    set(CPACK_RPM_PACKAGE_REQUIRES "devdeps-libaio >= 0.3.112, oceanbase-ce-devel = 3.1.2")
endif()
# install cpack to make everything work
include(CPack)

#add rpm target to create RPMS
add_custom_target(rpm
        COMMAND +make package
        DEPENDS
        logproxy)