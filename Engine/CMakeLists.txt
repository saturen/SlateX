enable_language(C)

file(GLOB_RECURSE ENGINE_SOURCES "*.cpp")
file(GLOB_RECURSE ENGINE_C_SOURCES "*.c")

add_library(Engine STATIC ${ENGINE_SOURCES} ${ENGINE_C_SOURCES})

target_include_directories(Engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GNS_INCLUDE}
    ${LUA_INCLUDE}
    ${SOL2_INCLUDE}
)

if(WIN32)
    target_link_libraries(Engine PUBLIC
        ${GNS_LIB}
        "${CONTRIB_DIR}/OpenSSL/lib/libcrypto.lib"
        "${CONTRIB_DIR}/OpenSSL/lib/libssl.lib"
        "${CONTRIB_DIR}/protobuf/lib/libprotobuf.lib"
        ${LUA_LIB}
        ws2_32
        crypt32
    )
    target_compile_definitions(Engine PRIVATE
        ENGINE_EXPORTS
        STEAMNETWORKINGSOCKETS_STATIC_LINK
    )
    set_property(TARGET Engine PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
else()
    target_link_libraries(Engine PUBLIC
        ${GNS_LIB}
        ssl
        crypto
        protobuf
        ${LUA_LIB}
        curl
    )
    target_compile_definitions(Engine PRIVATE
        ENGINE_EXPORTS
        STEAMNETWORKINGSOCKETS_STATIC_LINK
    )
endif()