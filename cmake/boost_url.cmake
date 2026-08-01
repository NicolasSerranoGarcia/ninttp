find_package(Boost 1.81 QUIET CONFIG COMPONENTS url)

if(NOT TARGET Boost::url)
    if(NOT NINTTP_FETCH_BOOST_URL)
        message(FATAL_ERROR
            "ninttp requires Boost.URL 1.81 or newer. Install Boost.URL or enable NINTTP_FETCH_BOOST_URL.")
    endif()

    include(FetchContent)

    set(BOOST_ENABLE_CMAKE ON)
    set(BOOST_SKIP_INSTALL_RULES ON)

    if(POLICY CMP0135)
        cmake_policy(SET CMP0135 NEW)
    endif()
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()

    FetchContent_Declare(boost
        URL https://archives.boost.io/release/1.91.0/source/boost_1_91_0.tar.gz
        URL_HASH SHA256=5734305f40a76c30f951c9abd409a45a2a19fb546efe4162119250bbe4d3a463
    )

    FetchContent_GetProperties(boost)
    if(NOT boost_POPULATED)
        FetchContent_Populate(boost)
    endif()

    # The release archive contains all Boost headers but no root CMake project. Supply
    # Boost.URL's header-only dependency targets, then build its upstream subdirectory.
    add_library(ninttp_boost_headers INTERFACE)
    target_include_directories(ninttp_boost_headers INTERFACE "${boost_SOURCE_DIR}")

    foreach(boost_url_header_dependency
        align
        assert
        config
        core
        mp11
        optional
        system
        throw_exception
        type_traits
        variant2)
        if(NOT TARGET Boost::${boost_url_header_dependency})
            add_library(Boost::${boost_url_header_dependency} ALIAS ninttp_boost_headers)
        endif()
    endforeach()

    set(BOOST_URL_BUILD_TESTS OFF)
    set(BOOST_URL_BUILD_EXAMPLES OFF)
    add_subdirectory(
        "${boost_SOURCE_DIR}/libs/url"
        "${boost_BINARY_DIR}/libs/url"
        EXCLUDE_FROM_ALL)
endif()

target_link_libraries(ninttp
    INTERFACE
        "$<BUILD_INTERFACE:Boost::url>"
)
