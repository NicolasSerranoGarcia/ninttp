add_executable(local_hello_world local_hello_world.cpp)

target_compile_features(local_hello_world
    PRIVATE
        cxx_std_20
)

target_link_libraries(local_hello_world
    PRIVATE
        ninttp::ninttp
)

