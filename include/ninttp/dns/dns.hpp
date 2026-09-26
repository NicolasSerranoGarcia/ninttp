#pragma once

#include "resolved_addresses.hpp"
#include "../socket/internal/select_backend.hpp"
#include <cstdint>
#include <expected>
#include <string_view>

namespace ninttp {
    class dns {
    public:
        // Blocking IPv4/IPv6 lookup. Errors retain the selected backend's resolver codes.
        static std::expected<ResolvedAddresses, internal::SelectedBackend::ErrorT> resolve(
            std::string_view hostname, std::uint16_t port = 0)
        {
            return internal::SelectedBackend::resolve(hostname, port);
        }
    };
}
