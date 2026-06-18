file(GLOB_RECURSE CLIENT_SOURCES "*.cpp")

add_executable(SlateXClient ${CLIENT_SOURCES})

target_include_directories(SlateXClient PRIVATE
    ${GNS_INCLUDE}
    ${RAYLIB_INCLUDE}
)

if(WIN32)
    set_property(TARGET SlateXClient PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    add_custom_command(TARGET SlateXClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONTRIB_DIR}/GNS-sdk/bin/GameNetworkingSockets.dll"
            $<TARGET_FILE_DIR:SlateXClient>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONTRIB_DIR}/protobuf/bin/libprotobuf.dll"
            $<TARGET_FILE_DIR:SlateXClient>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONTRIB_DIR}/OpenSSL/bin/libcrypto-3-x64.dll"
            $<TARGET_FILE_DIR:SlateXClient>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONTRIB_DIR}/OpenSSL/bin/libssl-3-x64.dll"
            $<TARGET_FILE_DIR:SlateXClient>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONTRIB_DIR}/Lua/bin/lua54.dll"
            $<TARGET_FILE_DIR:SlateXClient>
    )
endif()

target_link_libraries(SlateXClient PRIVATE Network Rendering)

add_custom_command(TARGET SlateXClient POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Content"
        "$<TARGET_FILE_DIR:SlateXClient>/Content"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/AppSettings.ini"
        "$<TARGET_FILE_DIR:SlateXClient>"
)