# RFC 9110 Sections 9.1 and 5.6.2 define method names as case-sensitive tokens.
# This list is used only to reject standard names from the extension-method option.
set(ninttp_standard_http_methods
    GET
    HEAD
    POST
    PUT
    DELETE
    CONNECT
    OPTIONS
    TRACE
    PATCH
)

set(ninttp_http_extension_methods ${NINTTP_HTTP_EXTENSION_METHODS})
list(REMOVE_DUPLICATES ninttp_http_extension_methods)

set(NINTTP_CONFIG_HTTP_EXTENSION_METHOD_COUNT 0)
set(NINTTP_CONFIG_HTTP_EXTENSION_METHOD_ENTRIES "")

foreach(method IN LISTS ninttp_http_extension_methods)
    if(NOT method MATCHES "^[A-Za-z0-9!#$%&'*+.^_`|~-]+$")
        message(FATAL_ERROR
            "NINTTP_HTTP_EXTENSION_METHODS contains '${method}', which is not a valid HTTP method token")
    endif()

    if(method IN_LIST ninttp_standard_http_methods)
        message(FATAL_ERROR
            "NINTTP_HTTP_EXTENSION_METHODS contains standard method '${method}'")
    endif()

    string(APPEND NINTTP_CONFIG_HTTP_EXTENSION_METHOD_ENTRIES
        "        std::string_view{\"${method}\"},\n")
    math(EXPR NINTTP_CONFIG_HTTP_EXTENSION_METHOD_COUNT
        "${NINTTP_CONFIG_HTTP_EXTENSION_METHOD_COUNT} + 1")
endforeach()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ninttp-http-method-config.hpp.in"
    "${ninttp_generated_include_dir}/ninttp/ninttp_http_method_config.hpp"
    @ONLY
)
