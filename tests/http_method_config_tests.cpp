#include "test_check.hpp"

#include "ninttp/http/http_method_config.hpp"

int main(){
    using namespace ninttp::internal;

    NINTTP_CHECK(isSupportedHttpMethod("GET"));
    NINTTP_CHECK(!isSupportedHttpMethod("gEt"));

    for(const auto method : configuredHttpExtensionMethods){
        NINTTP_CHECK(isConfiguredHttpExtensionMethod(method));
        NINTTP_CHECK(isSupportedHttpMethod(method));
    }
}
