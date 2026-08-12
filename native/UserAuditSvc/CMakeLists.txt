add_executable(UserAudit
    src/main.cpp
    src/service_host.cpp
    src/event_loop.cpp
    src/event_serializer.cpp
    src/jsonl_writer.cpp
    src/paths.cpp
    src/session_collector.cpp
    src/process_collector.cpp
    src/foreground_collector.cpp
    src/pipe_server.cpp
    src/pipe_client.cpp
    src/session_agent_manager.cpp
    src/user_agent_mode.cpp
    src/time_utils.cpp
)

target_include_directories(UserAudit PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(UserAudit PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_compile_definitions(UserAudit PRIVATE
    $<$<CONFIG:Debug>:USERAUDIT_DEV_CONSOLE=1>
)

target_link_libraries(UserAudit PRIVATE
    Advapi32
    Ole32
    Shell32
    Psapi
    Userenv
    Wtsapi32
    wevtapi
    rpcrt4
    tdh
)

if(MSVC)
    target_compile_options(UserAudit PRIVATE /W4 /permissive-)
endif()

install(TARGETS UserAudit RUNTIME DESTINATION bin)
