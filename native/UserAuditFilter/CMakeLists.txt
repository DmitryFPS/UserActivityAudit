# Optional WDK driver build. Requires USERAUDIT_BUILD_DRIVER=ON and installed WDK.
cmake_minimum_required(VERSION 3.20)

if(NOT USERAUDIT_BUILD_DRIVER)
    return()
endif()

file(GLOB WDK_KM_HEADERS "C:/Program Files (x86)/Windows Kits/10/Include/*/km/fltkernel.h")
if(NOT WDK_KM_HEADERS)
    message(WARNING "WDK kernel headers not found — skipping UserAuditFilter.sys (see docs/DRIVER.md)")
    return()
endif()

message(STATUS "UserAuditFilter: WDK detected — add MSBuild/EWDK project manually for .sys output")
message(STATUS "  Source: ${CMAKE_CURRENT_SOURCE_DIR}/UserAuditFilter.c")
message(STATUS "  INF:    ${CMAKE_CURRENT_SOURCE_DIR}/UserAuditFilter.inf")
