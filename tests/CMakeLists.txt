add_executable(ninttp_ninendian_tests
    nintraits_tests.cpp
)

add_executable(ninttp_ninendian_edge_tests
    nintraits_edge_tests.cpp
)

add_executable(ninttp_endpoints_invariant_tests
    endpoints_invariant_tests.cpp
)

add_executable(ninttp_http_method_config_tests
    http_method_config_tests.cpp
)

add_executable(ninttp_http_router_tests
    http_router_tests.cpp
)

target_link_libraries(ninttp_ninendian_tests
    PRIVATE
        ninttp::ninttp
)

target_link_libraries(ninttp_ninendian_edge_tests
    PRIVATE
        ninttp::ninttp
)

target_link_libraries(ninttp_endpoints_invariant_tests
    PRIVATE
        ninttp::ninttp
)

target_link_libraries(ninttp_http_method_config_tests
    PRIVATE
        ninttp::ninttp
)

target_link_libraries(ninttp_http_router_tests
    PRIVATE
        ninttp::ninttp
)

add_test(
    NAME ninttp.ninendian
    COMMAND ninttp_ninendian_tests
)

add_test(
    NAME ninttp.ninendian.edges
    COMMAND ninttp_ninendian_edge_tests
)

add_test(
    NAME ninttp.endpoints.invariant
    COMMAND ninttp_endpoints_invariant_tests
)

add_test(
    NAME ninttp.http.methods.config
    COMMAND ninttp_http_method_config_tests
)

add_test(
    NAME ninttp.http.router
    COMMAND ninttp_http_router_tests
)
