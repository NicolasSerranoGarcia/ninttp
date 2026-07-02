/**
 * @file select_backend.hpp
 * @author Nicolas Serrano (serranogarcianicolas@gmail.com)
 * @brief Selects the native socket backend for the current platform.
 * @version 0.1
 * @date 2026-06-26
 *
 * @copyright Copyright (c) 2026 Nicolas Serrano Garcia
 *
 */

#pragma once

/*
 * Backend headers are included outside the ninttp namespace because each backend
 * declares its own namespace. Including them inside namespace ninttp::internal
 * would nest the backend declarations as ninttp::internal::ninttp::internal.
 */
#include "../../platform_traits.hpp"

#if NINTTP_BACKEND_WINSOCK == 1
    #include "backends/winsock.hpp"
#elif NINTTP_BACKEND_POSIX == 1
    #include "backends/posix.hpp"
#else
    #error "ninttp: No supported socket backend selected."
#endif

namespace ninttp::internal
{
    #if NINTTP_BACKEND_WINSOCK == 1
        using SelectedBackend = WinsockBackend;
    #elif NINTTP_BACKEND_POSIX == 1
        using SelectedBackend = PosixBackend;
    #endif
} // namespace ninttp::internal
