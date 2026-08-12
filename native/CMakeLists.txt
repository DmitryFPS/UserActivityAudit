cmake_minimum_required(VERSION 3.20)

project(UserActivityAudit VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows-only client stack
if(NOT WIN32)
    message(FATAL_ERROR "UserActivityAudit native components require Windows.")
endif()

option(USERAUDIT_BUILD_DRIVER "Build minifilter driver (requires WDK)" OFF)

enable_testing()

add_subdirectory(common)
add_subdirectory(UserAuditSvc)
add_subdirectory(UserAuditWatchdog)
add_subdirectory(UserAuditKeygen)
add_subdirectory(UserAuditAdmin)
add_subdirectory(tests)

# Minifilter requires WDK — see docs/DRIVER.md and native/UserAuditFilter/
if(USERAUDIT_BUILD_DRIVER)
    add_subdirectory(UserAuditFilter)
endif()
