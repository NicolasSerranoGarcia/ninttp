- Document socket state lifetimes and invariants.
- Add tests for malformed HTTP messages, methods, framing combinations, and routing edge cases.
- Implement the chosen readiness-driven event loop:
  - add a poller abstraction for the supported backends;
  - register the listener and accepted sockets with the poller;
  - synchronize each connection's calculated `interests()` with its poller registration;
  - dispatch readiness to `onReadable()` and `onWritable()` instead of busy-scanning every connection.
- Define connection admission and backpressure limits, including a maximum active-connection count and bounded queued output.
- Continue header and folder cleanup as components stabilize.
- replace assert in code with safer alternatives for release builds

## HTTP/1.x RFC compliance roadmap

Main references:

- RFC 9110: HTTP Semantics
- RFC 9111: HTTP Caching
- RFC 9112: HTTP/1.1

### Methods

- Add client APIs and protocol behavior for methods beyond `GET`; routing already recognizes the standard methods and configured extension methods.
- Implement automatic method semantics where the library owns behavior:
  - `HEAD` sends headers only;
  - `OPTIONS *` works;

### Status and error responses

- Expand the existing parse-error mapping (`400`, `414`, and `505`) for:
  - `411 Length Required`;
  - `413 Content Too Large`;
  - `431 Request Header Fields Too Large`.
- Avoid response bodies for methods/status codes that forbid them.

### Header semantics

- Add case-insensitive header lookup helpers.
- Add structured helpers for important fields:
  - `Host`;
  - `Content-Length`;
  - `Transfer-Encoding`;
  - `TE`;
  - `Trailer`;
  - `Expect`;
  - `Date`;
  - `Server`;
  - `User-Agent`;
  - `Accept`;
  - `Content-Type`;
  - `Content-Encoding`;
  - `Range`;
  - conditional headers such as `If-Match`, `If-None-Match`, `If-Modified-Since`, and `If-Range`;
  - `Authorization`.
- Handle hop-by-hop headers correctly if proxy support is added.
- Generate `Date` on origin server responses when required/recommended.

### Client behavior

- Add TLS support before claiming `https` support.
- When TLS is added, validate the relationship between SNI and `Host`.

### Server behavior

- Complete readiness-based concurrent connection scheduling; accepted sockets are already nonblocking and each connection owns its parser, buffers, and exchange state.
- If static/file responses are added, support:
  - MIME type;
  - content length;
  - range requests;
  - conditional requests.

### Optional compliance areas

- Implement HTTP caching behavior from RFC 9111 only if the library becomes a cache or proxy:
  - `Cache-Control`;
  - `Expires`;
  - validators;
  - freshness;
  - revalidation.
- Implement proxy behavior only if proxy mode becomes a goal:
  - absolute-form request targets;
  - `CONNECT` authority-form request targets;
  - `Via`;
  - hop-by-hop field removal;
  - forwarding rules.
- Add authentication helpers:
  - `WWW-Authenticate`;
  - `Authorization`;
  - proxy authentication.
- Add content negotiation helpers:
  - `Accept`;
  - `Accept-Encoding`;
  - `Accept-Language`.
