cmake_minimum_required(VERSION 3.20)

project(UserActivityAudit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows-only client stack
if(NOT WIN32)
    message(FATAL_ERROR "UserActivityAudit native components require Windows.")
endif()

enable_testing()

add_subdirectory(UserAuditSvc)
add_subdirectory(UserAuditWatchdog)
add_subdirectory(tests)

# Phase 5+: add_subdirectory(UserAuditFilter)
