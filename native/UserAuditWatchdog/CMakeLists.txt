add_executable(UserAuditWatchdog
    main.cpp
)

target_compile_definitions(UserAuditWatchdog PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(UserAuditWatchdog PRIVATE Advapi32)

if(MSVC)
    target_compile_options(UserAuditWatchdog PRIVATE /W4 /permissive-)
endif()

install(TARGETS UserAuditWatchdog RUNTIME DESTINATION bin)
