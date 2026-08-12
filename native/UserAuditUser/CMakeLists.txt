add_executable(UserAuditUser
    src/main.cpp
    ../UserAuditSvc/src/foreground_collector.cpp
    ../UserAuditSvc/src/pipe_client.cpp
    ../UserAuditSvc/src/event_serializer.cpp
    ../UserAuditSvc/src/paths.cpp
    ../UserAuditSvc/src/time_utils.cpp
)

target_include_directories(UserAuditUser PRIVATE
    ../UserAuditSvc/include
)

target_compile_definitions(UserAuditUser PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    $<$<CONFIG:Debug>:USERAUDIT_DEV_CONSOLE=1>
)

target_link_libraries(UserAuditUser PRIVATE
    Psapi
    rpcrt4
    Wtsapi32
)

if(MSVC)
    target_compile_options(UserAuditUser PRIVATE /W4 /permissive-)
endif()

install(TARGETS UserAuditUser RUNTIME DESTINATION bin)
