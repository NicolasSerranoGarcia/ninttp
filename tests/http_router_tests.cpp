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
            response.body = request.target;
        }));

    ninttp::Request request;
    request.headers["host"] = "example.test";
    request.target = "/resource";
    request.method = "GET";

    auto matched = router.handleRequest(request);
    assert(matched.has_value());

    ninttp::Response response;
    matched->get()(request, response);
    assert(response.body == "/resource");

    request.method = "POST";
    auto disallowed = router.handleRequest(request);
    assert(!disallowed.has_value());
    assert(disallowed.error() == 405);
    assert(router.getAllowedMethods(request) == "GET");

    request.method = "gEt";
    auto unsupported = router.handleRequest(request);
    assert(!unsupported.has_value());
    assert(unsupported.error() == 501);

    request.method = "GET";
    request.target = "/missing";
    auto missingTarget = router.handleRequest(request);
    assert(!missingTarget.has_value());
    assert(missingTarget.error() == 404);

    request.headers["host"] = "unknown.test";
    auto unknownHost = router.handleRequest(request);
    assert(!unknownHost.has_value());
    assert(unknownHost.error() == 421);
}
