#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <utility>

#include "../types.hpp"
#include "../../error/nin_error.hpp"

namespace ninttp::internal{
    template<httpMethod method>
    struct httpMethodTraits{
        static constexpr std::string_view token = "";
        static constexpr bool acceptsContent = false;
    };

    template<>
    struct httpMethodTraits<httpMethod::GET>{
        static constexpr std::string_view token = allHttpMethodsStr[static_cast<std::size_t>(httpMethod::GET)];
        static constexpr bool acceptsContent = false;
    };

    template<>
    struct httpMethodTraits<httpMethod::PUT>{
        static constexpr std::string_view token = allHttpMethodsStr[static_cast<std::size_t>(httpMethod::PUT)];
        static constexpr bool acceptsContent = true;
    };

    template<>
    struct httpMethodTraits<httpMethod::DEL>{
        static constexpr std::string_view token = allHttpMethodsStr[static_cast<std::size_t>(httpMethod::DEL)];
        static constexpr bool acceptsContent = false;
    };

    template<>
    struct httpMethodTraits<httpMethod::PATCH>{
        static constexpr std::string_view token = allHttpMethodsStr[static_cast<std::size_t>(httpMethod::PATCH)];
        static constexpr bool acceptsContent = true;
    };

    template<>
    struct httpMethodTraits<httpMethod::HEAD>{
        static constexpr std::string_view token = allHttpMethodsStr[static_cast<std::size_t>(httpMethod::HEAD)];
        static constexpr bool acceptsContent = false;
    };

    template<>
    struct httpMethodTraits<httpMethod::OPTION>{
        static constexpr std::string_view token = allHttpMethodsStr[static_cast<std::size_t>(httpMethod::OPTION)];
        static constexpr bool acceptsContent = false;
    };

    template<>
    struct httpMethodTraits<httpMethod::POST>{
        static constexpr std::string_view token = allHttpMethodsStr[static_cast<std::size_t>(httpMethod::POST)];
        static constexpr bool acceptsContent = true;
    };

    template<>
    struct httpMethodTraits<httpMethod::CONNECT>{
        static constexpr std::string_view token = allHttpMethodsStr[static_cast<std::size_t>(httpMethod::CONNECT)];
        static constexpr bool acceptsContent = false;
    };

    template<>
    struct httpMethodTraits<httpMethod::TRACE>{
        static constexpr std::string_view token = allHttpMethodsStr[static_cast<std::size_t>(httpMethod::TRACE)];
        static constexpr bool acceptsContent = false;
    };

    //TODO: for more general use, assert that get is called only when there is a host and a target. for now it is only being used by the GET client
    template<httpVersion ver = http_1_0, httpMethod method = httpMethod::GET>
    class httpRequestBuilder{
        public:
            httpRequestBuilder(){
                request.method = method;
                request.version = ver;
            }

            std::expected<void, NinError> setContent(const std::string& content)
                requires(httpMethodTraits<method>::acceptsContent)
            {
                request.body = content;
                request.headers["content-length"] = std::to_string(content.size());
                return {};
            }

            std::expected<void, NinError> setHost(std::string_view host){
                request.headers["host"] = std::string(host);
                return {};
            }

            std::expected<void, NinError> setTarget(std::string_view target){
                request.target = target;
                return {};
            }

            [[nodiscard]] const Request& get() const & noexcept{
                return request;
            }

            [[nodiscard]] Request get() && noexcept{
                return std::move(request);
            }

            [[nodiscard]] static constexpr std::string_view methodToken() noexcept{
                return httpMethodTraits<method>::token;
            }

        private:
            Request request;
    };
} //namespace ninttp::internal
