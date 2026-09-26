#pragma once

#include "../endpoints.hpp"
#include <vector>

namespace ninttp {
    // Preserves resolver ordering within each family, but not between families.
    struct ResolvedAddresses {
        std::vector<IPv4Endpoint> ipv4;
        std::vector<IPv6Endpoint> ipv6;
    };
}
