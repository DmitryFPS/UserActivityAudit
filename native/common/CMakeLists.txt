cmake_minimum_required(VERSION 3.20)

add_library(UserAuditEd25519 STATIC
    ed25519_orlp/src/add_scalar.c
    ed25519_orlp/src/fe.c
    ed25519_orlp/src/ge.c
    ed25519_orlp/src/keypair.c
    ed25519_orlp/src/sc.c
    ed25519_orlp/src/seed.c
    ed25519_orlp/src/sha512.c
    ed25519_orlp/src/sign.c
    ed25519_orlp/src/verify.c
)

target_include_directories(UserAuditEd25519 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ed25519_orlp/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(UserAuditEd25519 PUBLIC Crypt32)

if(MSVC)
    target_compile_options(UserAuditEd25519 PRIVATE /W4 /wd4244 /wd4267)
endif()

add_library(UserAuditCommon STATIC
    src/auth_protocol.cpp
)

target_include_directories(UserAuditCommon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(UserAuditCommon PUBLIC UserAuditEd25519)

if(MSVC)
    target_compile_options(UserAuditCommon PRIVATE /W4 /permissive-)
endif()
