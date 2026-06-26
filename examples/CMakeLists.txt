add_executable(http_client http_client.cpp)
add_executable(http_server http_server.cpp)
add_executable(http_server_v6 http_server_v6.cpp)
add_executable(http_client_v6 http_client_v6.cpp)

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

target_compile_features(http_server_v6
    PRIVATE
        cxx_std_20
)

target_link_libraries(http_server_v6
    PRIVATE
        ninttp::ninttp
)

target_compile_features(http_client_v6
    PRIVATE
        cxx_std_20
)

target_link_libraries(http_client_v6
    PRIVATE
        ninttp::ninttp
)