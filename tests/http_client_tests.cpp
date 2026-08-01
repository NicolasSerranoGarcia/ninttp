#include <string_view>

#include <ninttp/http/http_client.hpp>

#include "test_check.hpp"

int main(){
    const auto expectInvalidAuthority = [](std::string_view authority){
        try{
            ninttp::httpClient client{
                ninttp::IPv4Endpoint::loopback(1),
                authority};
            (void)client;
            return false;
        } catch(const ninttp::NinError& error){
            return error.type == ninttp::NinErrorType::Parse &&
                error.parseErrorType == ninttp::internal::httpParseErrorType::InvalidAuthority;
        }
    };

    NINTTP_CHECK(expectInvalidAuthority(""));
    NINTTP_CHECK(expectInvalidAuthority("user@example.test"));
    NINTTP_CHECK(expectInvalidAuthority("example.test:99999"));
}
