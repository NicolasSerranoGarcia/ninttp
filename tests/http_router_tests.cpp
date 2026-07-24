#include <string>

#include "ninttp/http/internal/http_router.hpp"
#include "test_check.hpp"

int main(){
    ninttp::internal::httpRouter router;

    NINTTP_CHECK(router.registerHost("example.test"));
    NINTTP_CHECK(router.registerHandler(
        "example.test",
        "/resource",
        "GET",
        [](const ninttp::Request& request, ninttp::Response& response){
            response.setContent(request.getTarget());
        }));

    ninttp::Request request;
    NINTTP_CHECK(request.setHeader("host", "example.test"));
    request.setTarget("/resource");
    request.setMethod("GET");

    auto matched = router.handleRequest(request);
    NINTTP_CHECK(matched.has_value());

    ninttp::Response response;
    matched->get()(request, response);
    NINTTP_CHECK(response.getContent() == "/resource");

    request.setMethod("POST");
    auto disallowed = router.handleRequest(request);
    NINTTP_CHECK(!disallowed.has_value());
    NINTTP_CHECK(disallowed.error() == 405);
    NINTTP_CHECK(router.getAllowedMethods(request) == "GET");

    request.setMethod("gEt");
    auto unsupported = router.handleRequest(request);
    NINTTP_CHECK(!unsupported.has_value());
    NINTTP_CHECK(unsupported.error() == 501);

    request.setMethod("GET");
    request.setTarget("/missing");
    auto missingTarget = router.handleRequest(request);
    NINTTP_CHECK(!missingTarget.has_value());
    NINTTP_CHECK(missingTarget.error() == 404);

    NINTTP_CHECK(request.setHeader("host", "unknown.test"));
    auto unknownHost = router.handleRequest(request);
    NINTTP_CHECK(!unknownHost.has_value());
    NINTTP_CHECK(unknownHost.error() == 421);
}
