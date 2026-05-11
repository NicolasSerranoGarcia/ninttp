add_library(ninttp_win32_library INTERFACE)

target_link_libraries(ninttp_win32_library INTERFACE
    wsock32
    ws2_32
)
