#pragma once

#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace ninttp::internal{
    struct HeaderField{
        std::string name;
        std::string value;

        bool operator==(const HeaderField& other) const noexcept{
            return name == other.name && value == other.value;
        }

        [[nodiscard]] bool nameEquals(std::string_view other) const noexcept{
            if(name.size() != other.size())
                return false;

            for(std::size_t idx = 0; idx < name.size(); ++idx){
                char left = name[idx];
                char right = other[idx];

                if(left >= 'A' && left <= 'Z')
                    left = static_cast<char>(left + ('a' - 'A'));
                if(right >= 'A' && right <= 'Z')
                    right = static_cast<char>(right + ('a' - 'A'));

                if(left != right)
                    return false;
            }

            return true;
        }
    };
} // namespace ninttp::internal

namespace ninttp
{

    struct httpVersion{
        static constexpr std::optional<httpVersion> fromRequestLineVersion(const std::string& s){
            if (s == "HTTP/0.9") return httpVersion{0,9};
            if (s == "HTTP/1.0") return httpVersion{1,0};
            if (s == "HTTP/1.1") return httpVersion{1,1};
            if (s == "HTTP/2.0") return httpVersion{2,0};
            if (s == "HTTP/3.0") return httpVersion{3,0};
            return std::nullopt;
        }

        static constexpr std::optional<httpVersion> fromRequestLineVersion(std::string_view s) noexcept{
            if (s == "HTTP/0.9") return httpVersion{0,9};
            if (s == "HTTP/1.0") return httpVersion{1,0};
            if (s == "HTTP/1.1") return httpVersion{1,1};
            if (s == "HTTP/2.0") return httpVersion{2,0};
            if (s == "HTTP/3.0") return httpVersion{3,0};
            return std::nullopt;
        }

        constexpr httpVersion(std::uint8_t major, std::uint8_t minor) noexcept
            : major(major), minor(minor){}

        std::string toString() const noexcept{
            return std::to_string(major) + std::string(".") + std::to_string(minor);
        }

        std::string toHeaderString() const{
            return std::string("HTTP/") + toString();
        }

        httpVersion() noexcept = default;
        std::uint8_t major;
        std::uint8_t minor;
    };

    constexpr const httpVersion http_1_0(1,0);
    constexpr const httpVersion http_1_1(1,1);

    enum class RequestBodyFraming{
        None,
        ContentLength,
        Chunked
    };

    namespace internal{
        template<httpVersion ver>
        class httpRequestParser;

        template<httpVersion ver>
        class httpResponseParser;
    }

    using StatusCode = std::uint16_t;
    [[nodiscard]] constexpr std::string_view getReadableStatus(StatusCode code) noexcept{
        switch(code){
            case 100: return "Continue";
            case 101: return "Switching Protocols";
            case 102: return "Processing";
            case 103: return "Early Hints";
            case 200: return "OK";
            case 201: return "Created";
            case 202: return "Accepted";
            case 203: return "Non-Authoritative Information";
            case 204: return "No Content";
            case 205: return "Reset Content";
            case 206: return "Partial Content";
            case 207: return "Multi-Status";
            case 208: return "Already Reported";
            case 226: return "IM Used";
            case 300: return "Multiple Choices";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 303: return "See Other";
            case 304: return "Not Modified";
            case 305: return "Use Proxy";
            case 307: return "Temporary Redirect";
            case 308: return "Permanent Redirect";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 402: return "Payment Required";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 406: return "Not Acceptable";
            case 407: return "Proxy Authentication Required";
            case 408: return "Request Timeout";
            case 409: return "Conflict";
            case 410: return "Gone";
            case 411: return "Length Required";
            case 412: return "Precondition Failed";
            case 413: return "Content Too Large";
            case 414: return "URI Too Long";
            case 415: return "Unsupported Media Type";
            case 416: return "Range Not Satisfiable";
            case 417: return "Expectation Failed";
            case 418: return "I'm a teapot";
            case 421: return "Misdirected Request";
            case 422: return "Unprocessable Content";
            case 423: return "Locked";
            case 424: return "Failed Dependency";
            case 425: return "Too Early";
            case 426: return "Upgrade Required";
            case 428: return "Precondition Required";
            case 429: return "Too Many Requests";
            case 431: return "Request Header Fields Too Large";
            case 451: return "Unavailable For Legal Reasons";
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            case 504: return "Gateway Timeout";
            case 505: return "HTTP Version Not Supported";
            case 506: return "Variant Also Negotiates";
            case 507: return "Insufficient Storage";
            case 508: return "Loop Detected";
            case 510: return "Not Extended";
            case 511: return "Network Authentication Required";
            default: return "Unknown Status";
        }
    }

    class Response{
        public:
            using Headers = std::vector<internal::HeaderField>;

            constexpr Response() noexcept = default;

            constexpr Response(httpVersion responseVersion, StatusCode responseStatus) noexcept
                : version(responseVersion), statusCode(responseStatus){}

            [[nodiscard]] constexpr httpVersion getVersion() const noexcept{
                return version;
            }

            [[nodiscard]] constexpr StatusCode getStatusCode() const noexcept{
                return statusCode;
            }

            [[nodiscard]] const Headers& getHeaders() const noexcept{
                return headers;
            }

            [[nodiscard]] const std::optional<std::string>& getContent() const noexcept{
                return body;
            }

            constexpr void setVersion(httpVersion responseVersion) noexcept{
                version = responseVersion;
            }

            constexpr void setStatusCode(StatusCode responseStatus) noexcept{
                statusCode = responseStatus;
            }

            bool addHeader(std::string name, std::string value){
                if(internal::HeaderField{.name = name}.nameEquals("content-length") ||
                    internal::HeaderField{.name = name}.nameEquals("transfer-encoding"))
                    return false;

                headers.push_back(internal::HeaderField{
                    .name = std::move(name),
                    .value = std::move(value)
                });
                return true;
            }

            bool setHeader(std::string name, std::string value){
                if(internal::HeaderField{.name = name}.nameEquals("content-length") ||
                    internal::HeaderField{.name = name}.nameEquals("transfer-encoding"))
                    return false;

                for(auto& header : headers){
                    if(header.nameEquals(name)){
                        header.name = std::move(name);
                        header.value = std::move(value);
                        return true;
                    }
                }

                headers.push_back(internal::HeaderField{
                    .name = std::move(name),
                    .value = std::move(value)
                });
                return true;
            }

            void setContent(std::string content){
                body = std::move(content);
                setManagedHeader("Content-Length", std::to_string(body->size()));
            }

            void clearContent(){
                body.reset();
                setManagedHeader("Content-Length", "0");
            }

            [[nodiscard]] std::string toString() const{
                std::string responseStr;

                responseStr += version.toHeaderString();
                responseStr += " ";
                responseStr += std::to_string(statusCode);
                responseStr += " ";
                responseStr += getReadableStatus(statusCode);
                responseStr += "\r\n";

                for(const auto& header : headers){
                    responseStr += header.name;
                    responseStr += ": ";
                    responseStr += header.value;
                    responseStr += "\r\n";
                }

                responseStr += "\r\n";

                if(body.has_value())
                    responseStr += body.value();

                return responseStr;
            }

            friend inline std::ostream& operator<<(std::ostream& os, const Response& response){
                os << response.version.toHeaderString() << ' '
                   << response.statusCode << ' '
                   << getReadableStatus(response.statusCode) << '\n';

                for(const auto& header : response.headers)
                    os << header.name << ": " << header.value << '\n';

                if(response.body.has_value())
                    os << '\n' << response.body.value();

                return os;
            }

        private:
            template<httpVersion>
            friend class internal::httpResponseParser;

            void setManagedHeader(std::string name, std::string value){
                for(auto& header : headers){
                    if(header.nameEquals(name)){
                        header.name = std::move(name);
                        header.value = std::move(value);
                        return;
                    }
                }

                headers.push_back(internal::HeaderField{
                    .name = std::move(name),
                    .value = std::move(value)
                });
            }

            httpVersion version = http_1_0;
            StatusCode statusCode = 200;
            Headers headers;
            std::optional<std::string> body;
    };

    class Request{
        public:
            using Headers = std::unordered_map<std::string, std::string>;
            using TrailingHeaders = std::vector<internal::HeaderField>;

            [[nodiscard]] const std::string& getMethod() const noexcept{
                return method;
            }

            [[nodiscard]] const std::string& getTarget() const noexcept{
                return target;
            }

            [[nodiscard]] constexpr httpVersion getVersion() const noexcept{
                return version;
            }

            [[nodiscard]] const Headers& getHeaders() const noexcept{
                return headers;
            }

            [[nodiscard]] const std::optional<std::string>& getContent() const noexcept{
                return body;
            }

            [[nodiscard]] const std::optional<TrailingHeaders>& getTrailingHeaders() const noexcept{
                return trailingHeaders;
            }

            [[nodiscard]] constexpr RequestBodyFraming getBodyFraming() const noexcept{
                return bodyFraming;
            }

            void setMethod(std::string requestMethod){
                method = std::move(requestMethod);
            }

            void setTarget(std::string requestTarget){
                target = std::move(requestTarget);
            }

            constexpr void setVersion(httpVersion requestVersion) noexcept{
                version = requestVersion;
            }

            bool setHeader(std::string name, std::string value){
                normalizeHeaderName(name);

                if(name == "content-length" || name == "transfer-encoding")
                    return false;

                headers.insert_or_assign(std::move(name), std::move(value));
                return true;
            }

            bool setContent(std::string content, RequestBodyFraming framing = RequestBodyFraming::ContentLength){
                if(framing == RequestBodyFraming::None && !content.empty())
                    return false;

                headers.erase("content-length");
                headers.erase("transfer-encoding");
                body = std::move(content);
                bodyFraming = framing;

                switch(bodyFraming){
                    case RequestBodyFraming::None:
                        body.reset();
                        trailingHeaders.reset();
                        break;
                    case RequestBodyFraming::ContentLength:
                        headers["content-length"] = std::to_string(body->size());
                        trailingHeaders.reset();
                        break;
                    case RequestBodyFraming::Chunked:
                        headers["transfer-encoding"] = "chunked";
                        break;
                }

                return true;
            }

            void clearContent(){
                headers.erase("content-length");
                headers.erase("transfer-encoding");
                body.reset();
                trailingHeaders.reset();
                bodyFraming = RequestBodyFraming::None;
            }

            bool addTrailingHeader(std::string name, std::string value){
                if(bodyFraming != RequestBodyFraming::Chunked)
                    return false;

                normalizeHeaderName(name);
                if(name.empty())
                    return false;

                if(!trailingHeaders.has_value())
                    trailingHeaders.emplace();

                trailingHeaders->push_back(internal::HeaderField{
                    .name = std::move(name),
                    .value = std::move(value)
                });
                return true;
            }

            [[nodiscard]] bool hasConsistentBodyFraming() const noexcept{
                const auto contentLength = headers.find("content-length");
                const auto transferEncoding = headers.find("transfer-encoding");
                const auto bodyLength = body.has_value() ? body->size() : 0;

                switch(bodyFraming){
                    case RequestBodyFraming::None:
                        return contentLength == headers.end() &&
                            transferEncoding == headers.end() &&
                            !trailingHeaders.has_value() &&
                            (!body.has_value() || body->empty());

                    case RequestBodyFraming::ContentLength:{
                        if(contentLength == headers.end() ||
                            transferEncoding != headers.end() ||
                            trailingHeaders.has_value())
                            return false;

                        std::size_t declaredLength = 0;
                        const char* first = contentLength->second.data();
                        const char* last = first + contentLength->second.size();
                        const auto parsed = std::from_chars(first, last, declaredLength);

                        return parsed.ec == std::errc{} &&
                            parsed.ptr == last &&
                            declaredLength == bodyLength;
                    }

                    case RequestBodyFraming::Chunked:
                        return contentLength == headers.end() &&
                            transferEncoding != headers.end() &&
                            transferEncoding->second == "chunked";
                }

                return false;
            }

            [[nodiscard]] std::string toString() const{
                assert(hasConsistentBodyFraming());

                std::string requestStr;

                requestStr += method;
                requestStr += " ";
                requestStr += target;
                requestStr += " ";
                requestStr += version.toHeaderString();
                requestStr += "\r\n";

                for(const auto& header : headers){
                    requestStr += header.first;
                    requestStr += ": ";
                    requestStr += header.second;
                    requestStr += "\r\n";
                }

                const auto bodyLength = body.has_value() ? body->size() : 0;

                requestStr += "\r\n";

                switch(bodyFraming){
                    case RequestBodyFraming::None:
                        break;
                    case RequestBodyFraming::ContentLength:
                        if(body.has_value())
                            requestStr += body.value();
                        break;
                    case RequestBodyFraming::Chunked:{
                        if(bodyLength != 0){
                            char encodedLength[sizeof(std::size_t) * 2];
                            const auto encoded = std::to_chars(
                                encodedLength,
                                encodedLength + sizeof(encodedLength),
                                bodyLength,
                                16);

                            requestStr.append(encodedLength, encoded.ptr);
                            requestStr += "\r\n";
                            requestStr += body.value();
                            requestStr += "\r\n";
                        }

                        requestStr += "0\r\n";

                        if(trailingHeaders.has_value()){
                            for(const auto& trailer : trailingHeaders.value()){
                                requestStr += trailer.name;
                                requestStr += ": ";
                                requestStr += trailer.value;
                                requestStr += "\r\n";
                            }
                        }

                        requestStr += "\r\n";
                        break;
                    }
                }

                return requestStr;
            }

            friend inline std::ostream& operator<<(std::ostream& os, const Request& request){
                os << request.method << ' ' << request.target << '\n';

                for(const auto& header : request.headers)
                    os << header.first << ": " << header.second << '\n';

                if(request.body.has_value())
                    os << '\n' << request.body.value();

                return os;
            }

        private:
            template<httpVersion>
            friend class internal::httpRequestParser;

            static void normalizeHeaderName(std::string& name) noexcept{
                for(char& c : name){
                    if(c >= 'A' && c <= 'Z')
                        c = static_cast<char>(c + ('a' - 'A'));
                }
            }

            void reset(){
                method.clear();
                target.clear();
                version = http_1_0;
                headers.clear();
                body.reset();
                trailingHeaders.reset();
                bodyFraming = RequestBodyFraming::None;
            }

            std::string method;
            std::string target;
            httpVersion version = http_1_0;
            Headers headers;
            std::optional<std::string> body;
            std::optional<TrailingHeaders> trailingHeaders;
            RequestBodyFraming bodyFraming = RequestBodyFraming::None;
    };
} // namespace ninttp
