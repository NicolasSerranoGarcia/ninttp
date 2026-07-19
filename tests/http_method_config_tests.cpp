#include <cassert>

#include "ninttp/http/http_method_config.hpp"

int main(){
    using namespace ninttp::internal;

    assert(isSupportedHttpMethod("GET"));
    assert(!isSupportedHttpMethod("gEt"));

    for(const auto method : configuredHttpExtensionMethods){
        assert(isConfiguredHttpExtensionMethod(method));
        assert(isSupportedHttpMethod(method));
    }
}
