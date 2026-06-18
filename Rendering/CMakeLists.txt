file(GLOB_RECURSE RENDERING_SOURCES "*.cpp")

add_library(Rendering STATIC ${RENDERING_SOURCES})

target_include_directories(Rendering PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${RAYLIB_INCLUDE}
)

target_link_libraries(Rendering PUBLIC Engine)

if(WIN32)
    target_link_libraries(Rendering PUBLIC ${RAYLIB_LIB})
    set_property(TARGET Rendering PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
else()
    target_link_libraries(Rendering PUBLIC ${RAYLIB_LIB})
endif()