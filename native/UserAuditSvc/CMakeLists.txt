add_executable(UserAudit
    src/main.cpp
    src/service_host.cpp
    src/event_loop.cpp
    src/event_serializer.cpp
    src/jsonl_writer.cpp
    src/crypto_base64.cpp
    src/crypto_aes_gcm.cpp
    src/key_manager.cpp
    src/hash_chain.cpp
    src/log_crypto.cpp
    src/encrypted_log_writer.cpp
    src/decrypt_tool.cpp
    src/config_manager.cpp
    src/correlation_tracker.cpp
    src/file_collector.cpp
    src/network_collector.cpp
    src/print_collector.cpp
    src/clipboard_collector.cpp
    src/paths.cpp
    src/session_collector.cpp
    src/process_collector.cpp
    src/foreground_collector.cpp
    src/pipe_server.cpp
    src/pipe_client.cpp
    src/session_agent_manager.cpp
    src/usb_collector.cpp
    src/usb_helpers.cpp
    src/user_agent_mode.cpp
    src/time_utils.cpp
    src/acl_guard.cpp
    src/tamper_collector.cpp
    src/upload_client.cpp
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
    Bcrypt
    Crypt32
    Ole32
    Shell32
    Psapi
    Userenv
    Wtsapi32
    wevtapi
    rpcrt4
    tdh
    iphlpapi
    ws2_32
    winhttp
)

if(MSVC)
    target_compile_options(UserAudit PRIVATE /W4 /permissive-)
endif()

install(TARGETS UserAudit RUNTIME DESTINATION bin)
