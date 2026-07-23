#pragma once

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
        using HandlerT = std::function<void(const Request&, Response&)>;
        public:
            std::expected<std::reference_wrapper<const HandlerT>, StatusCode>
            handleRequest(const Request& request) const{
                const auto& headers = request.getHeaders();
                const auto hostHeader = headers.find("host");
                if(hostHeader == headers.end())
                    return std::unexpected{400};

                const auto hostIt = hosts_.find(hostHeader->second);
                if(hostIt == hosts_.end()){
                    return std::unexpected{421};
                }

                if(!internal::isSupportedHttpMethod(request.getMethod())){
                    return std::unexpected{501};
                }

                const auto targetIt = hostIt->second.find(request.getTarget());
                if(targetIt == hostIt->second.end()){
                    return std::unexpected{404};
                }

                const auto handlerIt = targetIt->second.find(request.getMethod());
                if(handlerIt == targetIt->second.end()){
                    return std::unexpected{405};
                }

                return std::cref(handlerIt->second);
            }

            [[nodiscard]] std::string getAllowedMethods(const Request& request) const{
                const auto& headers = request.getHeaders();
                const auto hostHeader = headers.find("host");
                if(hostHeader == headers.end())
                    return {};

                const auto hostIt = hosts_.find(hostHeader->second);
                if(hostIt == hosts_.end())
                    return {};

                const auto targetIt = hostIt->second.find(request.getTarget());
                if(targetIt == hostIt->second.end())
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
                if(host.empty())
                    return false;

                return hosts_.try_emplace(std::move(host)).second;
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

                auto hostIt = hosts_.find(host);
                if(hostIt == hosts_.end())
                    return false;

                hostIt->second[target].insert_or_assign(std::move(method), std::move(callback));
                return true;
            }
        private:
            using MethodHandlers = std::unordered_map<std::string, HandlerT>;
            using Targets = std::unordered_map<std::string, MethodHandlers>;
            using Hosts = std::unordered_map<std::string, Targets>;

            Hosts hosts_;
    };
}
