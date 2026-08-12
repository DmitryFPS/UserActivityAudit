add_executable(UserAuditSvc
    src/main.cpp
    src/service_host.cpp
    src/event_loop.cpp
    src/event_serializer.cpp
    src/jsonl_writer.cpp
    src/paths.cpp
    src/session_collector.cpp
    src/process_collector.cpp
    src/foreground_collector.cpp
    src/time_utils.cpp
)

target_include_directories(UserAuditSvc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(UserAuditSvc PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_compile_definitions(UserAuditSvc PRIVATE
    $<$<CONFIG:Debug>:USERAUDIT_DEV_CONSOLE=1>
)

target_link_libraries(UserAuditSvc PRIVATE
    Advapi32
    Ole32
    Shell32
    Psapi
    wevtapi
    rpcrt4
    tdh
)

if(MSVC)
    target_compile_options(UserAuditSvc PRIVATE /W4 /permissive-)
endif()

install(TARGETS UserAuditSvc RUNTIME DESTINATION bin)
