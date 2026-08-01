- Expand parser conformance coverage with fragmented-input matrices, malformed framing
  combinations, and request-smuggling edge cases.
- Implement the chosen readiness-driven event loop:
  - add a poller abstraction for the supported backends;
  - register the listener and accepted sockets with the poller;
  - synchronize each connection's calculated `interests()` with its poller registration;
  - dispatch readiness to `onReadable()` and `onWritable()` instead of busy-scanning every connection.
- Define connection admission and backpressure limits, including a maximum active-connection count and bounded queued output.
- Continue header and folder cleanup as components stabilize.
- Replace runtime `assert` contracts with safe release-build handling where failure is possible.

## HTTP/1.x RFC compliance roadmap

Main references:

- RFC 9110: HTTP Semantics
- RFC 9111: HTTP Caching
- RFC 9112: HTTP/1.1

### Methods

- Add client APIs and protocol behavior for methods beyond `GET`; routing already recognizes the standard methods and configured extension methods.
- Implement `OPTIONS *` and its automatic method semantics.

### Header semantics

- Add structured helpers for important fields:
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

### Server behavior

- If static/file responses are added, support:
  - MIME type;
  - content length;
  - range requests;
  - conditional requests.

### Optional compliance areas

- Add authentication helpers:
  - `WWW-Authenticate`;
  - `Authorization`;
  - proxy authentication.
- Add content negotiation helpers:
  - `Accept`;
  - `Accept-Encoding`;
  - `Accept-Language`.
