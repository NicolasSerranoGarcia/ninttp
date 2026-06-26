#internal variables, macros, objects are declared inversely to what it is the convention.
#For example, We will define a macro in uppercase if it is private, and lowercase if it is not.
#Same for variables

cmake_minimum_required(VERSION 3.14)
project(ninttp
    VERSION 0.0.1
    DESCRIPTION "An easy-to-use http server-client library written in C++"
    LANGUAGES CXX)

add_library(ninttp INTERFACE)
add_library(ninttp::ninttp ALIAS ninttp)
set(ninttp_export_targets ninttp)

string(COMPARE EQUAL
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    is_top_level)

# DEPENDENCIES

if(WIN32)

    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/win32.cmake)
    list(APPEND ninttp_export_targets ninttp_win32_library)

    target_link_libraries(ninttp INTERFACE ninttp_win32_library)

endif()


# BUILD OPTIONS

include(cmake/utils.cmake)
include(CheckIncludeFileCXX)
include(CheckCXXSymbolExists)
include(TestBigEndian)

# This is an internal CMake module (GNUInstallDirs.cmake) that unifies the paths for installation.
# It defines valuable variables such as CMAKE_INSTALL_INCLUDEDIR, CMAKE_INSTALL_LIBDIR, etc.
include(GNUInstallDirs)

option(NINTTP_BUILD_TESTS "Build ninttp tests" OFF)
option(NINTTP_BUILD_EXAMPLES "Build ninttp examples" OFF)
option(NINTTP_FD_CLOSE_EXEC "Mark POSIX socket file descriptors close-on-exec" ON)

# automatically set depending on if the project is being called as a subproject
# (in that case there is usually no need for an installation target)
option(NINTTP_INSTALL "Generate target for installing ninttp" ${is_top_level})

SET_IF_UNDEFINED(ninttp_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/ninttp-${PROJECT_VERSION}" CACHE STRING
    "Install path for ninttp package-related CMake files")

# PLATFORM/BACKEND DETECTION

check_include_file_cxx("winsock2.h" NINTTP_HAS_WINSOCK2_H)
check_include_file_cxx("ws2tcpip.h" NINTTP_HAS_WS2TCPIP_H)
check_include_file_cxx("sys/socket.h" NINTTP_HAS_SYS_SOCKET_H)
check_include_file_cxx("netinet/in.h" NINTTP_HAS_NETINET_IN_H)
check_include_file_cxx("arpa/inet.h" NINTTP_HAS_ARPA_INET_H)
check_include_file_cxx("unistd.h" NINTTP_HAS_UNISTD_H)
check_cxx_symbol_exists(accept4 "sys/socket.h" NINTTP_HAS_ACCEPT4)

set(NINTTP_CONFIG_PLATFORM_WINDOWS 0)
set(NINTTP_CONFIG_PLATFORM_UNIX 0)
set(NINTTP_CONFIG_PLATFORM_UNKNOWN 0)
set(NINTTP_CONFIG_PLATFORM_LINUX 0)
set(NINTTP_CONFIG_PLATFORM_APPLE 0)
set(NINTTP_CONFIG_PLATFORM_ANDROID 0)
set(NINTTP_CONFIG_PLATFORM_BSD 0)

set(NINTTP_CONFIG_BACKEND_WINSOCK 0)
set(NINTTP_CONFIG_BACKEND_POSIX 0)
set(NINTTP_CONFIG_SOCKET_BACKEND_REQUIRES_INIT 0)
set(NINTTP_CONFIG_SOCKET_INVALID_HANDLE_ALL_BITS_ONE 0)
set(NINTTP_CONFIG_SOCKET_INVALID_HANDLE_MINUS_ONE 0)
set(NINTTP_CONFIG_FD_CLOSE_EXEC 0)
set(NINTTP_CONFIG_HAVE_ACCEPT4 0)
set(NINTTP_CONFIG_BYTE_ORDER 0)

if(NINTTP_FD_CLOSE_EXEC)
    set(NINTTP_CONFIG_FD_CLOSE_EXEC 1)
endif()

if(NINTTP_HAS_ACCEPT4)
    set(NINTTP_CONFIG_HAVE_ACCEPT4 1)
endif()

test_big_endian(NINTTP_CONFIG_IS_BIG_ENDIAN)
if(NINTTP_CONFIG_IS_BIG_ENDIAN)
    set(NINTTP_CONFIG_BYTE_ORDER 4321)
else()
    set(NINTTP_CONFIG_BYTE_ORDER 1234)
endif()

if(WIN32 AND NINTTP_HAS_WINSOCK2_H)
    set(NINTTP_CONFIG_PLATFORM_WINDOWS 1)
    set(NINTTP_CONFIG_BACKEND_WINSOCK 1)
    set(NINTTP_CONFIG_SOCKET_BACKEND_REQUIRES_INIT 1)
    set(NINTTP_CONFIG_SOCKET_INVALID_HANDLE_ALL_BITS_ONE 1)
elseif(NINTTP_HAS_SYS_SOCKET_H AND NINTTP_HAS_NETINET_IN_H AND NINTTP_HAS_UNISTD_H)
    set(NINTTP_CONFIG_PLATFORM_UNIX 1)
    set(NINTTP_CONFIG_BACKEND_POSIX 1)
    set(NINTTP_CONFIG_SOCKET_BACKEND_REQUIRES_INIT 0)
    set(NINTTP_CONFIG_SOCKET_INVALID_HANDLE_MINUS_ONE 1)
else()
    set(NINTTP_CONFIG_PLATFORM_UNKNOWN 1)
    message(FATAL_ERROR "Could not detect a supported socket backend. Required: Winsock headers on Windows or POSIX socket headers on Unix-like systems.")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(NINTTP_CONFIG_PLATFORM_LINUX 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(NINTTP_CONFIG_PLATFORM_APPLE 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(NINTTP_CONFIG_PLATFORM_ANDROID 1)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|NetBSD|OpenBSD|DragonFly")
    set(NINTTP_CONFIG_PLATFORM_BSD 1)
endif()

set(ninttp_generated_include_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
file(MAKE_DIRECTORY "${ninttp_generated_include_dir}/ninttp")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ninttp-platform-config.hpp.in"
    "${ninttp_generated_include_dir}/ninttp/ninttp_platform_config.hpp"
    @ONLY
)

# LIBRARY HEADERS

set(public_headers
    include/ninttp/nintraits.hpp
    include/ninttp/socket/socket_error.hpp
    include/ninttp/socket/types.hpp
    include/ninttp/socket/utils.hpp
    include/ninttp/endpoints.hpp
    include/ninttp/socket/socket.hpp
    include/ninttp/socket/internal/select_backend.hpp
    include/ninttp/socket/internal/socket_state.hpp
    include/ninttp/socket/internal/socket_core.hpp
    include/ninttp/socket/internal/backends/concepts.hpp
    include/ninttp/socket/internal/backends/posix.hpp
    include/ninttp/socket/internal/backends/winsock.hpp
    include/ninttp/http/http_client.hpp
    include/ninttp/http/http_server.hpp
    include/ninttp/http/types.hpp
    include/ninttp/http/internal/http_request_builder.hpp
    include/ninttp/http/internal/http_request_parser.hpp
    include/ninttp/http/internal/http_response_builder.hpp
    include/ninttp/http/internal/http_response_parser.hpp
)

# groups have to match the location in which they are
# source_group is only for IDE/source tree organization; it does not affect the build itself.
# Since this is a header-only library, we group headers instead of compiled sources.
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${public_headers})

# LIBRARY TARGET

include(CMakePackageConfigHelpers)

# BUILD_INTERFACE and INSTALL_INTERFACE are not cmake variables.
# They only make sense inside generator expressions.
#
# It is important to note that CMake generator expressions ($<...>) are not evaluated
# during the configure phase (cmake ../). They are expanded during the generate phase
# (between the configure phase and the build phase), but these ones are special.
#
# $<BUILD_INTERFACE:...> is evaluated when the target is used directly from the build tree
# $<INSTALL_INTERFACE:...> is evaluated when exporting/installing the target via install(EXPORT ...)
#
# Since this is an INTERFACE target, INTERFACE is the correct visibility keyword:
# these include directories are not for building ninttp itself, but for anything that consumes it.
target_include_directories(ninttp
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${ninttp_generated_include_dir}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# If the library requires a minimum C++ standard, it should be expressed as an interface requirement.
# That way, consumers automatically inherit the correct language level
target_compile_features(ninttp
    INTERFACE
        cxx_std_23
)

# INSTALL TARGET - haven't gotten into depth with this part. I view it as kind of a default boilerplate
if(NINTTP_INSTALL)

    configure_package_config_file(cmake/ninttp-config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/ninttp-config.cmake"
        INSTALL_DESTINATION "${ninttp_INSTALL_CMAKEDIR}"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ninttp-config-version.cmake"
        COMPATIBILITY SameMajorVersion
    )

    # Even though this is a header-only library and there is no binary artifact,
    # install(TARGETS ...) still makes sense because we want to export the INTERFACE target
    # so downstream users can do find_package(ninttp) and target_link_libraries(... ninttp::ninttp).
    install(TARGETS ${ninttp_export_targets}
        EXPORT ninttp_export
    )

    # For header-only libraries, the public headers are usually installed directly from include/.
    # This is simpler than using PUBLIC_HEADER target properties.
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    )

    install(FILES
        "${ninttp_generated_include_dir}/ninttp/ninttp_platform_config.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ninttp"
    )

    set(targets_file "ninttp-targets.cmake")

    install(EXPORT ninttp_export
        FILE "${targets_file}"
        DESTINATION "${ninttp_INSTALL_CMAKEDIR}"
        NAMESPACE ninttp::
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ninttp-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ninttp-config-version.cmake"
        DESTINATION "${ninttp_INSTALL_CMAKEDIR}"
    )

endif()

# TEST AND EXAMPLES

if(NINTTP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(NINTTP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
