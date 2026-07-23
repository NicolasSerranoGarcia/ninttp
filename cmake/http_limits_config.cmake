set(ninttp_http_limit_names
    MaxMethodLength
    MaxRequestTargetLength
    MaxChunkExtensionsLength
    HTTPVersionLength
    MaxRequestLineLength
    MaxStatusLineLength
    MaxHeaderNameLength
    MaxHeaderValueLength
    MaxHeaderLineLength
    MaxHeaderSectionLength
    MaxHeaderCount
    MaxTrailerLineLength
    MaxTrailerSectionLength
    MaxTrailerCount
    MaxBodyLength
    MaxChunkLength
    MaxChunkLineLength
    MaxFieldValueLength
    ReadBufferSize
    MaxServerBacklog
)

foreach(limit_name IN LISTS ninttp_http_limit_names)
    if(DEFINED ${limit_name} AND NOT "${${limit_name}}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR "${limit_name} must be a non-negative integer")
    endif()

    if(DEFINED ${limit_name})
        set("NINTTP_CONFIG_${limit_name}" "${${limit_name}}")
    endif()
endforeach()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ninttp-http-limits-config.hpp.in"
    "${ninttp_generated_include_dir}/ninttp/ninttp_http_limits_config.hpp"
    @ONLY
)
