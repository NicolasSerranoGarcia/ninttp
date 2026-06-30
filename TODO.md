- State lifetime and invariants of the sockets
- Finish basic version of the parsers and builders
- tests for all types of http messages: malformed, methods, combinations of options...
- change interface to use span, modules and maybe threads, coroutines or IPC.
- tidy headers, move into folders and reorder constructs

## HTTP/1.x RFC compliance roadmap

Main references:
- RFC 9110: HTTP Semantics
- RFC 9111: HTTP Caching
- RFC 9112: HTTP/1.1

### Core message parsing

- Strictly parse request lines: `METHOD SP request-target SP HTTP-version CRLF`.
- Strictly parse status lines: `HTTP-version SP status-code SP reason-phrase CRLF`.
- Fix HTTP version handling:
  - accept `HTTP/1.0` and `HTTP/1.1`;
  - reject unsupported versions cleanly;
- Validate header field names as HTTP tokens.
- Parse header values with correct optional whitespace handling.
- Treat header names as case-insensitive.
- Define duplicate-header behavior, including fields that can be combined and fields that cannot.
- Reject or explicitly handle obsolete folded header lines.
- Add configurable limits for:
  - start-line length;
  - header section size;
  - header count;
  - body size;
  - request-target length.
- Preserve unconsumed bytes after parsing one message so pipelined messages can be parsed correctly.

### Message body framing

- Implement full `Content-Length` validation:
  - non-negative decimal values;
  - duplicate matching values;
  - duplicate conflicting values as framing errors;
  - overflow-safe parsing.
- Implement `Transfer-Encoding`, especially `chunked`.
- Parse chunk sizes, chunk extensions, chunk data, final chunk, and trailer fields.
- Enforce the precedence rules between `Transfer-Encoding` and `Content-Length`.
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

- Require and validate the `Host` header for HTTP/1.1 requests.
- Parse all HTTP/1.1 request-target forms:
  - origin-form: `/path?query`;
  - absolute-form for proxy requests;
  - authority-form for `CONNECT`;
  - asterisk-form for `OPTIONS *`.
- Add URI parsing/normalization enough to route safely.
- Add virtual-host routing support if the server can accept multiple authorities.

### Methods

- Support the standard methods:
  - `GET`;
  - `HEAD`;
  - `POST`;
  - `PUT`;
  - `DELETE`;
  - `CONNECT`;
  - `OPTIONS`;
  - `TRACE`.
- Support extension methods as valid HTTP tokens instead of treating methods as a closed enum only.
- Implement automatic method semantics where the library owns behavior:
  - `HEAD` sends headers only;
  - `OPTIONS *` works;
  - unknown unsupported methods return `501`;
  - known but disallowed methods return `405` with `Allow`.

### Status and error responses

- Generate valid status lines and reason phrases.
- Add built-in protocol responses for parse/framing errors:
  - `400 Bad Request`;
  - `411 Length Required`;
  - `413 Content Too Large`;
  - `414 URI Too Long`;
  - `431 Request Header Fields Too Large`;
  - `501 Not Implemented`;
  - `505 HTTP Version Not Supported`.
- Close the connection after unrecoverable framing errors.
- Avoid response bodies for methods/status codes that forbid them.
- Separate HTTP protocol errors from `SocketError`.

### Header semantics

- Add case-insensitive header lookup helpers.
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

- Build valid requests, including `Host`.
- Parse all valid response body framing modes.
- Add a redirect policy or explicitly leave redirects to the caller.
- Support `Expect: 100-continue` or handle it predictably.
- Add TLS support before claiming `https` support.

### Server behavior

- Add concurrent connection handling.
- Extend the handler API to expose:
  - method;
  - target/path/query;
  - headers;
  - body or body stream;
  - response headers/status/body.
- Generate automatic protocol responses for malformed requests.
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
