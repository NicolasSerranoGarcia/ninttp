#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

#include "../http_method_config.hpp"
#include "../types.hpp"
#include "parse_utils.hpp"

namespace ninttp::internal{
    template<httpVersion ver = http_1_0>
    class httpRouter{
        static_assert(isSupportedHTTP1Version(ver),
            "HTTP router only supports HTTP/1.0 and HTTP/1.1");
        using HandlerT = std::function<void(const Request&, Response&)>;
        using MethodHandlers = std::unordered_map<std::string, HandlerT>;
        using Targets = std::unordered_map<std::string, MethodHandlers>;
        using Hosts = std::unordered_map<std::string, Targets>;

        public:
            std::expected<std::reference_wrapper<const HandlerT>, StatusCode>
            handleRequest(const Request& request) const{
                if(!request.getAuthority().has_value())
                    return std::unexpected{400};

                const auto* targets = findTargets(*request.getAuthority());
                if(targets == nullptr)
                    return std::unexpected{421};

                if(!internal::isSupportedHttpMethod(request.getMethod())){
                    return std::unexpected{501};
                }

                const auto targetIt = targets->find(request.getPath());
                if(targetIt == targets->end()){
                    return std::unexpected{404};
                }

                const auto handlerIt = targetIt->second.find(request.getMethod());
                if(handlerIt == targetIt->second.end()){
                    return std::unexpected{405};
                }

                return std::cref(handlerIt->second);
            }

            [[nodiscard]] std::string getAllowedMethods(const Request& request) const{
                if(!request.getAuthority().has_value())
                    return {};

                const auto* targets = findTargets(*request.getAuthority());
                if(targets == nullptr)
                    return {};

                const auto targetIt = targets->find(request.getPath());
                if(targetIt == targets->end())
                    return {};

                std::string allowed;
                for(const auto& [method, handler] : targetIt->second){
                    if(!allowed.empty())
                        allowed += ", ";
                    allowed += method;
                }

                return allowed;
            }

            [[nodiscard]] bool registerHost(std::string host){
                auto authority = Authority::parseHost(host);
                if(!authority.has_value())
                    return false;

                auto key = authority->routingKey();
                const auto [hostIt, inserted] = hosts_.try_emplace(key);
                if(inserted)
                    defaultHosts_.try_emplace(authority->effectivePort(), std::move(key));

                return inserted;
            }

            [[nodiscard]] bool setDefaultHost(std::string host){
                auto authority = Authority::parseHost(host);
                if(!authority.has_value())
                    return false;

                auto key = authority->routingKey();
                if(!hosts_.contains(key))
                    return false;

                defaultHosts_.insert_or_assign(authority->effectivePort(), std::move(key));
                return true;
            }

            [[nodiscard]] bool registerHandler(
                const std::string& host,
                const std::string& target,
                std::string method,
                HandlerT callback)
            {
                if(method.empty() || !callback)
                    return false;

                for(const char c : method){
                    if(!utils::isTChar(c))
                        return false;
                }

                if(!internal::isSupportedHttpMethod(method))
                    return false;

                auto authority = Authority::parseHost(host);
                if(!authority.has_value())
                    return false;

                auto parsedTarget = RequestTarget::parseOriginForm(target);
                if(!parsedTarget.has_value() || parsedTarget->query().has_value())
                    return false;

                auto hostIt = hosts_.find(authority->routingKey());
                if(hostIt == hosts_.end())
                    return false;

                hostIt->second[parsedTarget->path()].insert_or_assign(
                    std::move(method),
                    std::move(callback));
                return true;
            }

        private:
            [[nodiscard]] const Targets* findTargets(const Authority& authority) const{
                if(const auto exact = hosts_.find(authority.routingKey()); exact != hosts_.end())
                    return &exact->second;

                const auto defaultIt = defaultHosts_.find(authority.effectivePort());
                if(defaultIt == defaultHosts_.end())
                    return nullptr;

                const auto hostIt = hosts_.find(defaultIt->second);
                if(hostIt != hosts_.end())
                    return &hostIt->second;

                return nullptr;
            }

            Hosts hosts_;
            std::unordered_map<std::uint16_t, std::string> defaultHosts_;
    };
}
