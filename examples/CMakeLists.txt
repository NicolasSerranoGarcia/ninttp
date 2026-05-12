add_executable(local_hello_world local_hello_world.cpp)
add_executable(http_client http_client.cpp)
add_executable(http_server http_server.cpp)

target_compile_features(local_hello_world
    PRIVATE
        cxx_std_20
)

target_link_libraries(local_hello_world
    PRIVATE
        ninttp::ninttp
)

target_compile_features(http_client
    PRIVATE
        cxx_std_20
)

target_link_libraries(http_client
    PRIVATE
        ninttp::ninttp
)

target_compile_features(http_server
    PRIVATE
        cxx_std_20
)

target_link_libraries(http_server
    PRIVATE
        ninttp::ninttp
)

