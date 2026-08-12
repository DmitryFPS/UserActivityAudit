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
