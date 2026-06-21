file(GLOB_RECURSE RUNTIME_SOURCES "*.cpp")

add_library(Runtime STATIC ${RUNTIME_SOURCES})

target_include_directories(Runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Network уже PUBLIC-линкует Engine, так что один Network
# транзитивно тащит и Engine, и Network на всех, кто линкует Runtime
target_link_libraries(Runtime PUBLIC Network)