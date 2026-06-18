file(GLOB_RECURSE SXS_SOURCES "*.cpp")

add_executable(SXS ${SXS_SOURCES})

target_include_directories(SXS PRIVATE ${GNS_INCLUDE})

if(WIN32)
    set_property(TARGET SXS PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
    add_custom_command(TARGET SXS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONTRIB_DIR}/GNS-sdk/bin/GameNetworkingSockets.dll"
            $<TARGET_FILE_DIR:SXS>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONTRIB_DIR}/protobuf/bin/libprotobuf.dll"
            $<TARGET_FILE_DIR:SXS>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONTRIB_DIR}/OpenSSL/bin/libcrypto-3-x64.dll"
            $<TARGET_FILE_DIR:SXS>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONTRIB_DIR}/OpenSSL/bin/libssl-3-x64.dll"
            $<TARGET_FILE_DIR:SXS>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CONTRIB_DIR}/Lua/bin/lua54.dll"
            $<TARGET_FILE_DIR:SXS>
    )
    target_link_libraries(SXS PRIVATE -Wl,--whole-archive Engine Network -Wl,--whole-archive)
else()
    target_link_libraries(SXS PRIVATE -Wl,--whole-archive Engine Network -Wl,--no-whole-archive
        ${GNS_LIB} ssl crypto protobuf)
endif()

add_custom_command(TARGET SXS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Content"
        "$<TARGET_FILE_DIR:SXS>/Content"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/AppSettings.ini"
        "$<TARGET_FILE_DIR:SXS>"
)