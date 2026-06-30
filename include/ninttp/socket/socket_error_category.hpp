#pragma once

namespace ninttp{
    enum class SocketErrorCategory{
        Other, //swift enums would be soo clean here
        Blocks,
        Interrupted,
        ConnectionClosed,
        ConnectionLost
    };
}
