add_executable(UserAuditKeygen
    main.cpp
    ../UserAuditSvc/src/auth_guard.cpp
    ../UserAuditSvc/src/paths.cpp
)

target_include_directories(UserAuditKeygen PRIVATE
    ../UserAuditSvc/include
    ../common/include
    ../common/ed25519_orlp/src
)

target_link_libraries(UserAuditKeygen PRIVATE UserAuditCommon Ole32 Shell32 Bcrypt)

target_compile_definitions(UserAuditKeygen PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
    target_compile_options(UserAuditKeygen PRIVATE /W4 /permissive-)
endif()

install(TARGETS UserAuditKeygen RUNTIME DESTINATION bin)
