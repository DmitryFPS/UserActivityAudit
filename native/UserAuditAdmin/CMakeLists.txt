add_executable(UserAuditAdmin
    main.cpp
    ../UserAuditSvc/src/auth_guard.cpp
    ../UserAuditSvc/src/paths.cpp
)

target_include_directories(UserAuditAdmin PRIVATE
    ../UserAuditSvc/include
    ../common/include
    ../common/ed25519_orlp/src
)

target_link_libraries(UserAuditAdmin PRIVATE UserAuditCommon Ole32 Shell32 Bcrypt)

target_compile_definitions(UserAuditAdmin PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(UserAuditAdmin PRIVATE /W4 /permissive-)
endif()

install(TARGETS UserAuditAdmin RUNTIME DESTINATION bin)
