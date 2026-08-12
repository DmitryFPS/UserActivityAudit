add_executable(test_event_serializer
    test_event_serializer.cpp
    ../UserAuditSvc/src/event_serializer.cpp
)

target_include_directories(test_event_serializer PRIVATE
    ../UserAuditSvc/include
)

if(MSVC)
    target_compile_options(test_event_serializer PRIVATE /W4 /permissive-)
endif()

add_test(NAME test_event_serializer COMMAND test_event_serializer)

add_executable(test_usb_helpers
    test_usb_helpers.cpp
    ../UserAuditSvc/src/usb_helpers.cpp
)

target_include_directories(test_usb_helpers PRIVATE
    ../UserAuditSvc/include
)

if(MSVC)
    target_compile_options(test_usb_helpers PRIVATE /W4 /permissive-)
endif()

add_test(NAME test_usb_helpers COMMAND test_usb_helpers)

add_executable(test_log_crypto
    test_log_crypto.cpp
    ../UserAuditSvc/src/log_crypto.cpp
    ../UserAuditSvc/src/crypto_aes_gcm.cpp
    ../UserAuditSvc/src/crypto_base64.cpp
)

target_include_directories(test_log_crypto PRIVATE
    ../UserAuditSvc/include
)

target_link_libraries(test_log_crypto PRIVATE Bcrypt Crypt32)

if(MSVC)
    target_compile_options(test_log_crypto PRIVATE /W4 /permissive-)
endif()

add_test(NAME test_log_crypto COMMAND test_log_crypto)

add_executable(test_hash_chain
    test_hash_chain.cpp
    ../UserAuditSvc/src/hash_chain.cpp
)

target_include_directories(test_hash_chain PRIVATE
    ../UserAuditSvc/include
)

target_link_libraries(test_hash_chain PRIVATE Bcrypt)

if(MSVC)
    target_compile_options(test_hash_chain PRIVATE /W4 /permissive-)
endif()

add_test(NAME test_hash_chain COMMAND test_hash_chain)

add_executable(test_config_manager
    test_config_manager.cpp
    ../UserAuditSvc/src/config_manager.cpp
    ../UserAuditSvc/src/paths.cpp
)

target_include_directories(test_config_manager PRIVATE
    ../UserAuditSvc/include
)

target_link_libraries(test_config_manager PRIVATE Ole32 Shell32)

if(MSVC)
    target_compile_options(test_config_manager PRIVATE /W4 /permissive-)
endif()

add_test(NAME test_config_manager COMMAND test_config_manager)

add_executable(test_upload_client
    test_upload_client.cpp
    ../UserAuditSvc/src/config_manager.cpp
    ../UserAuditSvc/src/upload_client.cpp
    ../UserAuditSvc/src/paths.cpp
)

target_include_directories(test_upload_client PRIVATE
    ../UserAuditSvc/include
)

target_link_libraries(test_upload_client PRIVATE Ole32 Shell32 winhttp)

if(MSVC)
    target_compile_options(test_upload_client PRIVATE /W4 /permissive-)
endif()

add_test(NAME test_upload_client COMMAND test_upload_client)
