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

    NINTTP_CHECK(request.setTarget("/resource?q=hello%20world&tag=cpp&tag=http"));
    NINTTP_CHECK(request.setHeader("host", "EXAMPLE.TEST:80"));
    matched = router.handleRequest(request);
    NINTTP_CHECK(matched.has_value());
    NINTTP_CHECK(request.getPath() == "/resource");
    NINTTP_CHECK(request.getQuery() == "q=hello%20world&tag=cpp&tag=http");
    NINTTP_CHECK(request.getQueryParameters().size() == 3);
    NINTTP_CHECK(request.getQueryParameters()[0].key == "q");
    NINTTP_CHECK(request.getQueryParameters()[0].value == "hello world");
    NINTTP_CHECK(request.getQueryParameters()[1].key == "tag");
    NINTTP_CHECK(request.getQueryParameters()[1].value == "cpp");
    NINTTP_CHECK(request.getQueryParameters()[2].key == "tag");
    NINTTP_CHECK(request.getQueryParameters()[2].value == "http");

    NINTTP_CHECK(router.registerHost("example.test:8080"));
    NINTTP_CHECK(router.registerHandler(
        "example.test:8080",
        "/resource",
        "GET",
        [](const ninttp::Request&, ninttp::Response& exactResponse){
            exactResponse.setContent("exact-port");
        }));

    NINTTP_CHECK(request.setHeader("host", "example.test:8080"));
    matched = router.handleRequest(request);
    NINTTP_CHECK(matched.has_value());
    ninttp::Response exactResponse;
    matched->get()(request, exactResponse);
    NINTTP_CHECK(exactResponse.getContent() == "exact-port");

    NINTTP_CHECK(request.setHeader("host", "127.0.0.1:8080"));
    matched = router.handleRequest(request);
    NINTTP_CHECK(matched.has_value());

    NINTTP_CHECK(request.setHeader("host", "example.test:9090"));
    matched = router.handleRequest(request);
    NINTTP_CHECK(!matched.has_value());
    NINTTP_CHECK(matched.error() == 421);

    NINTTP_CHECK(router.registerHost("alternate.test:8080"));
    NINTTP_CHECK(router.registerHandler(
        "alternate.test:8080",
        "/resource",
        "GET",
        [](const ninttp::Request&, ninttp::Response& alternateResponse){
            alternateResponse.setContent("alternate-default");
        }));
    NINTTP_CHECK(router.setDefaultHost("alternate.test:8080"));
    NINTTP_CHECK(request.setHeader("host", "unknown.test:8080"));
    matched = router.handleRequest(request);
    NINTTP_CHECK(matched.has_value());
    ninttp::Response alternateResponse;
    matched->get()(request, alternateResponse);
    NINTTP_CHECK(alternateResponse.getContent() == "alternate-default");

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
    NINTTP_CHECK(request.setTarget("/missing"));
    auto missingTarget = router.handleRequest(request);
    NINTTP_CHECK(!missingTarget.has_value());
    NINTTP_CHECK(missingTarget.error() == 404);

    NINTTP_CHECK(request.setHeader("host", "unknown.test:9090"));
    auto unknownHost = router.handleRequest(request);
    NINTTP_CHECK(!unknownHost.has_value());
    NINTTP_CHECK(unknownHost.error() == 421);

    NINTTP_CHECK(!request.setHeader("host", "user@example.test"));
    NINTTP_CHECK(!request.setHeader("host", "example.test:99999"));
    NINTTP_CHECK(!request.setTarget("/bad%target"));
}
