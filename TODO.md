- Document socket state lifetimes and invariants.
- Finish the response parser and generalize the request/response builders beyond the current `Content-Length`-only path.
- Add tests for malformed HTTP messages, methods, framing combinations, and routing edge cases.
- Decide on the server concurrency model: threads, coroutines, an event loop, or external scheduling.
- Continue header and folder cleanup as components stabilize.

## HTTP/1.x RFC compliance roadmap

Main references:

- RFC 9110: HTTP Semantics
- RFC 9111: HTTP Caching
- RFC 9112: HTTP/1.1

### Core message parsing

- Strictly parse status lines: `HTTP-version SP status-code SP reason-phrase CRLF`.
- Validate response header field names as HTTP tokens; request header names are already validated.
- Parse header values with correct optional whitespace handling.
- Reject obsolete folded header lines explicitly in both parsers.
- Make the existing request-line, method, and request-target limits configurable.
- Add configurable limits for header section size, header count, body size, and response start-line length.

### Message body framing

- Implement `Transfer-Encoding`, especially `chunked`.
- Parse chunk sizes, chunk extensions, chunk data, final chunk, and trailer fields.
- Enforce all `Transfer-Encoding`/`Content-Length` framing rules in both parsers; the request parser currently rejects their coexistence.
- Support connection-close-delimited response bodies where allowed.
- Forbid response bodies where HTTP forbids them:
  - responses to `HEAD`;
  - `1xx`;
  - `204`;
  - `304`;
  - successful `CONNECT`.
- Handle request bodies for methods beyond `GET`.

### HTTP/1.1 connection behavior

- Make HTTP/1.1 connections persistent by default.
- Honor `Connection: close`.
- Decide whether to support HTTP/1.0 keep-alive compatibility and implement it consistently if so.
- Support request pipelining or deliberately serialize/reject it.
- Add read, write, idle, and incomplete-message timeouts.
- Define socket half-close and connection error behavior in protocol terms.

### Request routing and targets

- Complete `Host` handling. Missing and duplicate fields are already rejected.
  - Require `Host` only where the HTTP version requires it.
  - Parse the authority as a hostname, IPv4 address, or bracketed IPv6 literal, with an optional port.
  - Reject malformed and empty authorities with `400 Bad Request`.
  - Normalize hostnames and default ports for matching while preserving the received authority for diagnostics.
  - Define how an explicit default authority interacts with a request made directly to an IP address.
- Parse all HTTP/1.1 request-target forms:
  - origin-form: `/path?query`;
  - absolute-form for proxy requests;
  - authority-form for `CONNECT`;
  - asterisk-form for `OPTIONS *`.
- Add URI parsing/normalization enough to route safely.
- Complete virtual-host routing semantics; registration and `Host` -> target -> method lookup are in place.
  - Document the current `421 Misdirected Request` policy for unknown authorities or make it configurable.
  - Keep Host routing separate from authorization; a client-controlled Host value is not proof of identity.
  - When TLS is added, validate the relationship between SNI and `Host`.

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
- Close the connection after unrecoverable framing errors.
- Avoid response bodies for methods/status codes that forbid them.

### Header semantics

- Normalize response header names and add case-insensitive lookup helpers; request header names are already stored lowercase.
- Add structured helpers for important fields:
  - `Host`;
  - `Connection`;
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
- Generate either valid `Content-Length` or valid chunked encoding for outgoing bodies.

### Client behavior

- Validate client `Host` authorities and request targets; generated requests already include `Host`.
- Parse all valid response body framing modes.
- Add a redirect policy or explicitly leave redirects to the caller.
- Support `Expect: 100-continue` or handle it predictably.
- Add TLS support before claiming `https` support.

### Server behavior

- Add concurrent connection handling.
- Expose listener backlog through server configuration with a sensible default.
- Manage keep-alive connection lifetimes.
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
  - absolute-form routing;
  - `CONNECT`;
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
