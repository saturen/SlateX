file(GLOB_RECURSE NETWORK_SOURCES "*.cpp")

add_library(Network STATIC ${NETWORK_SOURCES})

target_include_directories(Network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GNS_INCLUDE}
)

target_link_libraries(Network PUBLIC Engine)

if(WIN32)
    target_compile_definitions(Network PRIVATE STEAMNETWORKINGSOCKETS_STATIC_LINK)
    set_property(TARGET Network PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
else()
    target_compile_definitions(Network PRIVATE STEAMNETWORKINGSOCKETS_STATIC_LINK)
endif()