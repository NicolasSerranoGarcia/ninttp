# ninttp
An easy-to-use http server-client library written in C++

## HTTP limits

HTTP parser and server limits have safe defaults in
`ninttp/http/http_limits.hpp`. Override only the values your application needs;
unspecified limits retain their defaults.

With CMake, pass the limit's exact name:

```sh
cmake -S . -B build \
  -DMaxBodyLength=134217728 \
  -DMaxHeaderCount=200
cmake --build build
```

When using ninttp through `add_subdirectory`, set the values before adding it:

```cmake
set(MaxBodyLength 134217728)
set(MaxHeaderCount 200)
add_subdirectory(path/to/ninttp)
```

When consuming the headers without ninttp's CMake project, use compiler
definitions with the same names:

```sh
c++ -DMaxBodyLength=134217728 -DMaxHeaderCount=200 ...
```

Configured values are available as constants in `ninttp::limits`, for example
`ninttp::limits::MaxBodyLength`. Limit values are byte counts unless their name
ends in `Count`; `MaxServerBacklog` is a connection count.

| Limit | Default | Controls |
| --- | ---: | --- |
| `MaxMethodLength` | 32 | Request method |
| `MaxRequestTargetLength` | 8000 | Request target |
| `HTTPVersionLength` | 8 | HTTP version token |
| `MaxRequestLineLength` | derived | Complete request line |
| `MaxStatusLineLength` | 8192 | Complete response status line |
| `MaxHeaderNameLength` | 256 | Header field name |
| `MaxHeaderValueLength` | 8192 | Parsed header field value |
| `MaxHeaderLineLength` | 16384 | One header line |
| `MaxHeaderSectionLength` | 65536 | Complete header section |
| `MaxHeaderCount` | 100 | Header fields |
| `MaxTrailerLineLength` | 16384 | One trailer line |
| `MaxTrailerSectionLength` | 65536 | Complete trailer section |
| `MaxTrailerCount` | 100 | Trailer fields |
| `MaxBodyLength` | 67108864 | Decoded message body |
| `MaxChunkLength` | 16777216 | One chunk |
| `MaxChunkLineLength` | 8224 | Chunk-size line including extensions |
| `MaxChunkExtensionsLength` | 8192 | Extensions on one chunk |
| `MaxFieldValueLength` | 256 | Client-configured field values such as Host |
| `ReadBufferSize` | 512 | Socket read buffer |
| `MaxServerBacklog` | 100 | Listener backlog |

If `MaxRequestLineLength` is not explicitly configured, its default is
recalculated from `MaxMethodLength`, `MaxRequestTargetLength`, and
`HTTPVersionLength`.

## HTTP extension methods

HTTP method names are case-sensitive tokens. ninttp recognizes the standard method names with
their exact spelling and can be configured at build time to recognize additional methods.

Pass extension methods as a semicolon-separated CMake cache value:

```sh
cmake -S . -B build \
  -DNINTTP_HTTP_EXTENSION_METHODS="PURGE;PROPFIND;M-SEARCH"
cmake --build build
```

When ninttp is included with `add_subdirectory`, set the cache value before adding it:

```cmake
set(NINTTP_HTTP_EXTENSION_METHODS "PURGE;PROPFIND;M-SEARCH" CACHE STRING
    "HTTP extension methods supported by ninttp")
add_subdirectory(path/to/ninttp)
```

The list is case-sensitive. For example, `PURGE`, `Purge`, and `purge` identify three different
methods. Every entry must satisfy the HTTP `token` grammar. Configuration fails if an entry is
invalid or duplicates a standard method with its exact spelling.

CMake generates `ninttp/ninttp_http_method_config.hpp` in the build tree. Applications should not
edit or include that generated file directly; ninttp accesses it through
`ninttp/http/http_method_config.hpp`. Reconfigure the project after changing the extension list.
When ninttp headers are used without CMake, the extension list is empty.

After configuration, register handlers using the same exact token:

```cpp
ninttp::httpServer server;

server.registerHost("localhost");
server.registerHandler(
    "localhost",
    "/cache",
    "PURGE",
    [](const ninttp::Request& request, ninttp::Response& response) {
        (void)request;
        response.body = "cache cleared";
    });
```

`registerHandler` returns `false` when the host has not been registered, the method is not a valid
token, the extension was not configured, or the handler is empty. Configuring an extension only
declares that the server recognizes it; the application remains responsible for implementing the
method's semantics safely.

At request time, a valid method token outside the standard and configured sets receives `501 Not
Implemented`. A supported method without a handler for an existing target receives `405 Method
Not Allowed`, with the target's installed methods listed in `Allow`.

### Standards basis

This behavior follows [RFC 9110 Section 9.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-9.1):

- the request method grammar is `method = token`;
- method tokens are case-sensitive;
- methods outside the specification can be defined and registered;
- an unrecognized or unimplemented method should receive `501`;
- a recognized and implemented method that is not allowed for the target should receive `405`.

The accepted token characters come from
[RFC 9110 Section 5.6.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-5.6.2).
Standardized extension methods should also be registered in the
[IANA HTTP Method Registry](https://www.iana.org/assignments/http-methods/http-methods.xhtml), as
described by RFC 9110 Section 16.1.1.
