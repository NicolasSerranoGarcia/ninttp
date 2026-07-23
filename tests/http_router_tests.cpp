#include <cassert>
#include <string>

#include "ninttp/http/internal/http_router.hpp"

int main(){
    ninttp::internal::httpRouter router;

    assert(router.registerHost("example.test"));
    assert(router.registerHandler(
        "example.test",
        "/resource",
        "GET",
        [](const ninttp::Request& request, ninttp::Response& response){
            response.setContent(request.getTarget());
        }));

    ninttp::Request request;
    assert(request.setHeader("host", "example.test"));
    request.setTarget("/resource");
    request.setMethod("GET");

    auto matched = router.handleRequest(request);
    assert(matched.has_value());

    ninttp::Response response;
    matched->get()(request, response);
    assert(response.getContent() == "/resource");

    request.setMethod("POST");
    auto disallowed = router.handleRequest(request);
    assert(!disallowed.has_value());
    assert(disallowed.error() == 405);
    assert(router.getAllowedMethods(request) == "GET");

    request.setMethod("gEt");
    auto unsupported = router.handleRequest(request);
    assert(!unsupported.has_value());
    assert(unsupported.error() == 501);

    request.setMethod("GET");
    request.setTarget("/missing");
    auto missingTarget = router.handleRequest(request);
    assert(!missingTarget.has_value());
    assert(missingTarget.error() == 404);

    assert(request.setHeader("host", "unknown.test"));
    auto unknownHost = router.handleRequest(request);
    assert(!unknownHost.has_value());
    assert(unknownHost.error() == 421);
}
