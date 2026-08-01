#include <iostream>

#include <boost/url.hpp>

int main(){
    const auto parsedTarget = boost::urls::parse_origin_form(
        "/search?q=hello%20world&tag=cpp&tag=http");
    if(!parsedTarget){
        std::cerr << "target parse failed: " << parsedTarget.error().message() << '\n';
        return 1;
    }

    const auto target = parsedTarget.value();
    std::cout << "request target\n"
              << "  path: " << target.encoded_path() << '\n'
              << "  query: " << target.encoded_query() << '\n';

    for(const auto parameter : target.params()){
        std::cout << "  parameter: " << parameter.key;
        if(parameter.has_value)
            std::cout << '=' << parameter.value;
        std::cout << '\n';
    }

    const auto parsedAuthority = boost::urls::parse_authority("localhost:8080");
    if(!parsedAuthority){
        std::cerr << "authority parse failed: " << parsedAuthority.error().message() << '\n';
        return 1;
    }

    const auto authority = parsedAuthority.value();
    std::cout << "authority\n"
              << "  host: " << authority.encoded_host() << '\n'
              << "  port: " << authority.port() << '\n';

    boost::urls::url generated;
    generated.set_scheme("http");
    generated.set_host("localhost");
    generated.set_port_number(8080);
    generated.set_path("/search");
    generated.params().append({"q", "hello world"});
    generated.params().append({"tag", "cpp"});

    std::cout << "generated URL\n"
              << "  " << generated << '\n';
}
