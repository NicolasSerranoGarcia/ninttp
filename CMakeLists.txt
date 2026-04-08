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

string(COMPARE EQUAL
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    is_top_level)

# BUILD OPTIONS

include(cmake/utils.cmake)

# This is an internal CMake module (GNUInstallDirs.cmake) that unifies the paths for installation.
# It defines valuable variables such as CMAKE_INSTALL_INCLUDEDIR, CMAKE_INSTALL_LIBDIR, etc.
include(GNUInstallDirs)

option(ninttp_BUILD_TESTS "Build ninttp tests" OFF)
option(ninttp_BUILD_EXAMPLES "Build ninttp examples" OFF)

# automatically set depending on if the project is being called as a subproject
# (in that case there is usually no need for an installation target)
option(ninttp_INSTALL "Generate target for installing ninttp" ${is_top_level})

SET_IF_UNDEFINED(ninttp_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/ninttp-${PROJECT_VERSION}" CACHE STRING
    "Install path for ninttp package-related CMake files")

# LIBRARY HEADERS

set(public_headers
    include/ninttp/ninttp.hpp
    include/ninttp/socket/interfaces.hpp
    include/ninttp/socket/errors.hpp
    include/ninttp/socket/types.hpp
    include/ninttp/socket/ninendian.hpp
    include/ninttp/socket/endpoints.hpp
    include/ninttp/socket/socket.hpp
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
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

# If the library requires a minimum C++ standard, it should be expressed as an interface requirement.
# That way, consumers automatically inherit the correct language level
target_compile_features(ninttp
    INTERFACE
        cxx_std_20
)

# INSTALL TARGET - haven't gotten into depth with this part. I view it as kind of a default boilerplate
if(ninttp_INSTALL)

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
    install(TARGETS ninttp
        EXPORT ninttp_export
    )

    # For header-only libraries, the public headers are usually installed directly from include/.
    # This is simpler than using PUBLIC_HEADER target properties.
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
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