#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <concepts>
#include <cstring>
#include <optional>
#include <string_view>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "../../types.hpp"
#include "../../utils.hpp"
#include "../../endpoints.hpp"

namespace ninttp::internal
{

    static constexpr inline int toNative(Domain domain) noexcept{
        switch (domain) {
            case Domain::IPv4: return AF_INET;
            case Domain::IPv6: return AF_INET6;
            default: return -1;
        }
    }
    
    static constexpr inline int toNative(Service service) noexcept{
        switch (service) {
            case Service::Stream: return SOCK_STREAM;
            case Service::Datagram: return SOCK_DGRAM;
            case Service::Raw: return SOCK_RAW;
            case Service::SeqPacket: return SOCK_SEQPACKET;
            default: return -1;
        }
    }
    
    static constexpr inline int toNative(Protocol protocol) noexcept{
        switch (protocol) {
            case Protocol::Default: return 0;
            case Protocol::Tcp: return IPPROTO_TCP;
            case Protocol::Udp: return IPPROTO_UDP;
            default: return -1;
        }
    }
    
    static constexpr inline int toNativeShutdownPolicy(ShutdownPolicy what) noexcept{
        switch(what){
            case ShutdownPolicy::SHUT_RECEPTIONS:
                return SD_RECEIVE;
            case ShutdownPolicy::SHUT_TRANSMISSIONS:
                return SD_SEND;
            case ShutdownPolicy::SHUT_TRANSMISSIONS_AND_RECEPTIONS:
                return SD_BOTH;
            default:
                return -1;
        }
    }


    class WinsockBackend{
        public:
            using ErrorT = int;
            using SocketT = SOCKET;
            using AddressStorageT = sockaddr_storage;
            using AddressLenT = int; //included from the own winsock2

            struct AddressBundleT{
                SocketT socket;
                AddressStorageT storage;
                AddressLenT len;
            };

            //init and deinit count is left as a chore for the user of the
            //functions, as one can init the backend several times
            //asking for different versions of the winsock
            static bool init() noexcept{
                WSADATA wsaData;

                int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
                if (iResult != 0) {
                    //since this is currently sync, we probably can get away with this
                    WSASetLastError(iResult);
                    return false;
                }

                return true;
            }

            static bool deinit() noexcept{
                if(WSACleanup() == SOCKET_ERROR)
                    return false;

                return true;
            }

            //returns a valid socket descriptor or invalidSocket() on error
            static SocketT openSocket(Domain d, Service s, Protocol p) noexcept{
                return static_cast<SocketT>(::socket(toNative(d), toNative(s), toNative(p)));
            }
            
            static bool closeSocket(const SocketT& s) noexcept{
                return ::closesocket(s) == 0;
            };
            
            static constexpr bool isValidSocket(const SocketT& s) noexcept{ return s != invalidSocket(); };
            
            static bool shutdownSocket(const SocketT& s, ShutdownPolicy what) noexcept{ 
                return ::shutdown(s, toNativeShutdownPolicy(what)) != SOCKET_ERROR;
            };

            static bool bind(const SocketT& s, const AddressStorageT* addr, AddressLenT len) noexcept{
                return ::bind(s, reinterpret_cast<const sockaddr*>(addr), len) != SOCKET_ERROR;
            }


            static bool listen(const SocketT& s, int backlog) noexcept{
                return ::listen(s, backlog) != SOCKET_ERROR;
            }

            static std::optional<AddressBundleT> accept(const SocketT& s) noexcept{
                AddressStorageT storage;
                AddressLenT len = sizeof(storage);

                SocketT sock = ::accept(s, reinterpret_cast<sockaddr*>(&storage), &len);

                if(sock == invalidSocket())
                    return {};

                return AddressBundleT{sock, storage, len};
            }

            static bool connect(const SocketT& s, const AddressStorageT* addr, AddressLenT len) noexcept{ 
                return ::connect(s, reinterpret_cast<const sockaddr*>(addr), len) != SOCKET_ERROR;
            }

            static AddressStorageT toStorage(const IPv4Endpoint& endpoint) noexcept{ 
                sockaddr_in native{};
                native.sin_family = AF_INET;
                native.sin_addr.s_addr = hostToNetwork32(endpoint.addressHostOrder());
                native.sin_port = hostToNetwork16(endpoint.portHostOrder());

                AddressStorageT storage{};
                std::memcpy(&storage, &native, sizeof(native));
                return storage;
            }

            static constexpr AddressLenT storageLen(const IPv4Endpoint&) noexcept{
                return static_cast<AddressLenT>(sizeof(sockaddr_in));
            }

            template<typename EndpointT>
            static EndpointT fromStorage(const AddressStorageT& storage){
                if constexpr(std::same_as<EndpointT, IPv4Endpoint>){
                    if(storage.ss_family != AF_INET)
                        throw std::runtime_error("Invalid IPv4 endpoint storage");

                    sockaddr_in native{};
                    std::memcpy(&native, &storage, sizeof(native));
                    return IPv4Endpoint(
                        networkToHost32(native.sin_addr.s_addr),
                        networkToHost16(native.sin_port));
                }else{
                    static_assert(std::same_as<EndpointT, IPv4Endpoint>, "Unsupported endpoint type");
                }
            }

            static ssize_t send(const SocketT& s, const char* buff, size_t n) noexcept{
                return ::send(s, buff, n, 0);
            }

            static ssize_t receive(const SocketT& s, char* buff, size_t n) noexcept{
                return ::recv(s, buff, n, 0);
            }

            static ErrorT getLastError() noexcept{ return static_cast<int>(WSAGetLastError()); }

            //FormatMessage can fail and in that case we should either throw
            //or return an optional
            static std::string getMsgFromError(const ErrorT& err){
                std::string msg(512, '\0');

                //TODO: windows also supports wide chars but return
                //type has assumed to be narrow char for posix 
                //so maybe define generic? Support for wide chars is 
                //not really a concern either way
                auto written = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL,
                    static_cast<int>(err),
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    msg.data(),
                    static_cast<DWORD>(msg.size()),
                    NULL
                );

                msg.resize(written);
                if(msg.empty())
                    return "Unknown Winsock error " + std::to_string(err);

                while(!msg.empty() && std::string_view{"\r\n "}.find(msg.back()) != std::string_view::npos)
                    msg.pop_back();

                return msg;
            }

            static constexpr inline SocketT invalidSocket() noexcept{ return INVALID_SOCKET; };
    };
} // namespace ninttp::internal
