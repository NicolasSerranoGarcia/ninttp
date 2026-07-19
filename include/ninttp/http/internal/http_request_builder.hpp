#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <utility>

#include "../types.hpp"
#include "../../error/nin_error.hpp"

namespace ninttp::internal{
    //TODO: for more general use, assert that get is called only when there is a host and a target. for now it is only being used by the GET client
    template<httpVersion ver = http_1_0>
    class httpRequestBuilder{
        public:
            explicit httpRequestBuilder(std::string_view method){
                request.method = method;
                request.version = ver;
            }

            std::expected<void, NinError> setContent(const std::string& content){
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

        private:
            Request request;
    };
} //namespace ninttp::internal
