#pragma once

#include <cstddef>

#if defined(__has_include)
    #if __has_include(<ninttp/ninttp_http_limits_config.hpp>)
        #include <ninttp/ninttp_http_limits_config.hpp>
    #endif
#endif

namespace ninttp::limits::detail{
#ifdef MaxMethodLength
    inline constexpr std::size_t ConfiguredMaxMethodLength = MaxMethodLength;
#elif defined(NINTTP_CONFIG_MaxMethodLength)
    inline constexpr std::size_t ConfiguredMaxMethodLength = NINTTP_CONFIG_MaxMethodLength;
#else
    inline constexpr std::size_t ConfiguredMaxMethodLength = 32;
#endif
#ifdef MaxRequestTargetLength
    inline constexpr std::size_t ConfiguredMaxRequestTargetLength = MaxRequestTargetLength;
#elif defined(NINTTP_CONFIG_MaxRequestTargetLength)
    inline constexpr std::size_t ConfiguredMaxRequestTargetLength = NINTTP_CONFIG_MaxRequestTargetLength;
#else
    inline constexpr std::size_t ConfiguredMaxRequestTargetLength = 8000;
#endif
#ifdef MaxChunkExtensionsLength
    inline constexpr std::size_t ConfiguredMaxChunkExtensionsLength = MaxChunkExtensionsLength;
#elif defined(NINTTP_CONFIG_MaxChunkExtensionsLength)
    inline constexpr std::size_t ConfiguredMaxChunkExtensionsLength = NINTTP_CONFIG_MaxChunkExtensionsLength;
#else
    inline constexpr std::size_t ConfiguredMaxChunkExtensionsLength = 8192;
#endif
#ifdef HTTPVersionLength
    inline constexpr std::size_t ConfiguredHTTPVersionLength = HTTPVersionLength;
#elif defined(NINTTP_CONFIG_HTTPVersionLength)
    inline constexpr std::size_t ConfiguredHTTPVersionLength = NINTTP_CONFIG_HTTPVersionLength;
#else
    inline constexpr std::size_t ConfiguredHTTPVersionLength = 8;
#endif
#ifdef MaxRequestLineLength
    inline constexpr std::size_t ConfiguredMaxRequestLineLength = MaxRequestLineLength;
#elif defined(NINTTP_CONFIG_MaxRequestLineLength)
    inline constexpr std::size_t ConfiguredMaxRequestLineLength = NINTTP_CONFIG_MaxRequestLineLength;
#else
    inline constexpr std::size_t ConfiguredMaxRequestLineLength =
        ConfiguredMaxMethodLength + ConfiguredMaxRequestTargetLength +
        ConfiguredHTTPVersionLength + 4;
#endif
#ifdef MaxStatusLineLength
    inline constexpr std::size_t ConfiguredMaxStatusLineLength = MaxStatusLineLength;
#elif defined(NINTTP_CONFIG_MaxStatusLineLength)
    inline constexpr std::size_t ConfiguredMaxStatusLineLength = NINTTP_CONFIG_MaxStatusLineLength;
#else
    inline constexpr std::size_t ConfiguredMaxStatusLineLength = 8192;
#endif
#ifdef MaxHeaderNameLength
    inline constexpr std::size_t ConfiguredMaxHeaderNameLength = MaxHeaderNameLength;
#elif defined(NINTTP_CONFIG_MaxHeaderNameLength)
    inline constexpr std::size_t ConfiguredMaxHeaderNameLength = NINTTP_CONFIG_MaxHeaderNameLength;
#else
    inline constexpr std::size_t ConfiguredMaxHeaderNameLength = 256;
#endif
#ifdef MaxHeaderValueLength
    inline constexpr std::size_t ConfiguredMaxHeaderValueLength = MaxHeaderValueLength;
#elif defined(NINTTP_CONFIG_MaxHeaderValueLength)
    inline constexpr std::size_t ConfiguredMaxHeaderValueLength = NINTTP_CONFIG_MaxHeaderValueLength;
#else
    inline constexpr std::size_t ConfiguredMaxHeaderValueLength = 8192;
#endif
#ifdef MaxHeaderLineLength
    inline constexpr std::size_t ConfiguredMaxHeaderLineLength = MaxHeaderLineLength;
#elif defined(NINTTP_CONFIG_MaxHeaderLineLength)
    inline constexpr std::size_t ConfiguredMaxHeaderLineLength = NINTTP_CONFIG_MaxHeaderLineLength;
#else
    inline constexpr std::size_t ConfiguredMaxHeaderLineLength = 16384;
#endif
#ifdef MaxHeaderSectionLength
    inline constexpr std::size_t ConfiguredMaxHeaderSectionLength = MaxHeaderSectionLength;
#elif defined(NINTTP_CONFIG_MaxHeaderSectionLength)
    inline constexpr std::size_t ConfiguredMaxHeaderSectionLength = NINTTP_CONFIG_MaxHeaderSectionLength;
#else
    inline constexpr std::size_t ConfiguredMaxHeaderSectionLength = 65536;
#endif
#ifdef MaxHeaderCount
    inline constexpr std::size_t ConfiguredMaxHeaderCount = MaxHeaderCount;
#elif defined(NINTTP_CONFIG_MaxHeaderCount)
    inline constexpr std::size_t ConfiguredMaxHeaderCount = NINTTP_CONFIG_MaxHeaderCount;
#else
    inline constexpr std::size_t ConfiguredMaxHeaderCount = 100;
#endif
#ifdef MaxTrailerLineLength
    inline constexpr std::size_t ConfiguredMaxTrailerLineLength = MaxTrailerLineLength;
#elif defined(NINTTP_CONFIG_MaxTrailerLineLength)
    inline constexpr std::size_t ConfiguredMaxTrailerLineLength = NINTTP_CONFIG_MaxTrailerLineLength;
#else
    inline constexpr std::size_t ConfiguredMaxTrailerLineLength = 16384;
#endif
#ifdef MaxTrailerSectionLength
    inline constexpr std::size_t ConfiguredMaxTrailerSectionLength = MaxTrailerSectionLength;
#elif defined(NINTTP_CONFIG_MaxTrailerSectionLength)
    inline constexpr std::size_t ConfiguredMaxTrailerSectionLength = NINTTP_CONFIG_MaxTrailerSectionLength;
#else
    inline constexpr std::size_t ConfiguredMaxTrailerSectionLength = 65536;
#endif
#ifdef MaxTrailerCount
    inline constexpr std::size_t ConfiguredMaxTrailerCount = MaxTrailerCount;
#elif defined(NINTTP_CONFIG_MaxTrailerCount)
    inline constexpr std::size_t ConfiguredMaxTrailerCount = NINTTP_CONFIG_MaxTrailerCount;
#else
    inline constexpr std::size_t ConfiguredMaxTrailerCount = 100;
#endif
#ifdef MaxBodyLength
    inline constexpr std::size_t ConfiguredMaxBodyLength = MaxBodyLength;
#elif defined(NINTTP_CONFIG_MaxBodyLength)
    inline constexpr std::size_t ConfiguredMaxBodyLength = NINTTP_CONFIG_MaxBodyLength;
#else
    inline constexpr std::size_t ConfiguredMaxBodyLength = 64 * 1024 * 1024;
#endif
#ifdef MaxChunkLength
    inline constexpr std::size_t ConfiguredMaxChunkLength = MaxChunkLength;
#elif defined(NINTTP_CONFIG_MaxChunkLength)
    inline constexpr std::size_t ConfiguredMaxChunkLength = NINTTP_CONFIG_MaxChunkLength;
#else
    inline constexpr std::size_t ConfiguredMaxChunkLength = 16 * 1024 * 1024;
#endif
#ifdef MaxChunkLineLength
    inline constexpr std::size_t ConfiguredMaxChunkLineLength = MaxChunkLineLength;
#elif defined(NINTTP_CONFIG_MaxChunkLineLength)
    inline constexpr std::size_t ConfiguredMaxChunkLineLength = NINTTP_CONFIG_MaxChunkLineLength;
#else
    inline constexpr std::size_t ConfiguredMaxChunkLineLength = 8224;
#endif
#ifdef MaxFieldValueLength
    inline constexpr std::size_t ConfiguredMaxFieldValueLength = MaxFieldValueLength;
#elif defined(NINTTP_CONFIG_MaxFieldValueLength)
    inline constexpr std::size_t ConfiguredMaxFieldValueLength = NINTTP_CONFIG_MaxFieldValueLength;
#else
    inline constexpr std::size_t ConfiguredMaxFieldValueLength = 256;
#endif
#ifdef ReadBufferSize
    inline constexpr std::size_t ConfiguredReadBufferSize = ReadBufferSize;
#elif defined(NINTTP_CONFIG_ReadBufferSize)
    inline constexpr std::size_t ConfiguredReadBufferSize = NINTTP_CONFIG_ReadBufferSize;
#else
    inline constexpr std::size_t ConfiguredReadBufferSize = 512;
#endif
#ifdef MaxServerBacklog
    inline constexpr int ConfiguredMaxServerBacklog = MaxServerBacklog;
#elif defined(NINTTP_CONFIG_MaxServerBacklog)
    inline constexpr int ConfiguredMaxServerBacklog = NINTTP_CONFIG_MaxServerBacklog;
#else
    inline constexpr int ConfiguredMaxServerBacklog = 100;
#endif
}

#ifdef MaxMethodLength
    #undef MaxMethodLength
#endif
#ifdef MaxRequestTargetLength
    #undef MaxRequestTargetLength
#endif
#ifdef MaxChunkExtensionsLength
    #undef MaxChunkExtensionsLength
#endif
#ifdef HTTPVersionLength
    #undef HTTPVersionLength
#endif
#ifdef MaxRequestLineLength
    #undef MaxRequestLineLength
#endif
#ifdef MaxStatusLineLength
    #undef MaxStatusLineLength
#endif
#ifdef MaxHeaderNameLength
    #undef MaxHeaderNameLength
#endif
#ifdef MaxHeaderValueLength
    #undef MaxHeaderValueLength
#endif
#ifdef MaxHeaderLineLength
    #undef MaxHeaderLineLength
#endif
#ifdef MaxHeaderSectionLength
    #undef MaxHeaderSectionLength
#endif
#ifdef MaxHeaderCount
    #undef MaxHeaderCount
#endif
#ifdef MaxTrailerLineLength
    #undef MaxTrailerLineLength
#endif
#ifdef MaxTrailerSectionLength
    #undef MaxTrailerSectionLength
#endif
#ifdef MaxTrailerCount
    #undef MaxTrailerCount
#endif
#ifdef MaxBodyLength
    #undef MaxBodyLength
#endif
#ifdef MaxChunkLength
    #undef MaxChunkLength
#endif
#ifdef MaxChunkLineLength
    #undef MaxChunkLineLength
#endif
#ifdef MaxFieldValueLength
    #undef MaxFieldValueLength
#endif
#ifdef ReadBufferSize
    #undef ReadBufferSize
#endif
#ifdef MaxServerBacklog
    #undef MaxServerBacklog
#endif

namespace ninttp::limits{
    inline constexpr std::size_t MaxMethodLength = detail::ConfiguredMaxMethodLength;
    inline constexpr std::size_t MaxRequestTargetLength = detail::ConfiguredMaxRequestTargetLength;
    inline constexpr std::size_t MaxChunkExtensionsLength = detail::ConfiguredMaxChunkExtensionsLength;
    inline constexpr std::size_t HTTPVersionLength = detail::ConfiguredHTTPVersionLength;
    inline constexpr std::size_t MaxRequestLineLength = detail::ConfiguredMaxRequestLineLength;
    inline constexpr std::size_t MaxStatusLineLength = detail::ConfiguredMaxStatusLineLength;
    inline constexpr std::size_t MaxHeaderNameLength = detail::ConfiguredMaxHeaderNameLength;
    inline constexpr std::size_t MaxHeaderValueLength = detail::ConfiguredMaxHeaderValueLength;
    inline constexpr std::size_t MaxHeaderLineLength = detail::ConfiguredMaxHeaderLineLength;
    inline constexpr std::size_t MaxHeaderSectionLength = detail::ConfiguredMaxHeaderSectionLength;
    inline constexpr std::size_t MaxHeaderCount = detail::ConfiguredMaxHeaderCount;
    inline constexpr std::size_t MaxTrailerLineLength = detail::ConfiguredMaxTrailerLineLength;
    inline constexpr std::size_t MaxTrailerSectionLength = detail::ConfiguredMaxTrailerSectionLength;
    inline constexpr std::size_t MaxTrailerCount = detail::ConfiguredMaxTrailerCount;
    inline constexpr std::size_t MaxBodyLength = detail::ConfiguredMaxBodyLength;
    inline constexpr std::size_t MaxChunkLength = detail::ConfiguredMaxChunkLength;
    inline constexpr std::size_t MaxChunkLineLength = detail::ConfiguredMaxChunkLineLength;
    inline constexpr std::size_t MaxFieldValueLength = detail::ConfiguredMaxFieldValueLength;
    inline constexpr std::size_t ReadBufferSize = detail::ConfiguredReadBufferSize;
    inline constexpr int MaxServerBacklog = detail::ConfiguredMaxServerBacklog;
}
