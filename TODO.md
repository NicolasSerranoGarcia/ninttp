# ninttp TODO (HTTP/1.0 first)

Project direction: build a solid HTTP/1.0 server first.

This means we should prioritize a clean TCP server API and defer Unix-socket “full surface” features that are not needed yet.

## Project structure needs

- [ ] Setup CMake and a folder structure that supports future changes
- [ ] Setup TODO as github projects and milestones

## Must-have for HTTP/1.0 MVP

### 1 Basic TCP connection flow

- [ ] Server bind/listen for IPv4 and IPv6 with clear failure errors.
- [ ] Accept one client connection reliably and expose peer IP/port.
- [ ] Client connect API stays simple and predictable.

### 2 Read/write data on connected sockets

- [ ] Add `recv` for stream sockets.
- [ ] Add `send` for stream sockets.
- [ ] Handle partial reads/writes correctly (loop until done / caller-controlled).
- [ ] Decide behavior for `EINTR` and `EAGAIN`.

### 3 Connection lifecycle

- [ ] Add public `close()` (explicit close, not only destructor close).
- [ ] Add `shutdown()` for graceful HTTP response ending.
- [ ] Make move/ownership semantics explicit (no accidental double-close).

### 4 Minimum socket options needed for web servers

- [ ] `SO_REUSEADDR` for quick restart during development.
- [ ] `SO_REUSEPORT` (optional, but useful for scaling/workers).
- [ ] Basic receive/send timeout options.
- [ ] `TCP_NODELAY` (optional toggle; default can stay off initially).

### 5 Error model that is usable in server code

- [ ] Replace `bool`-only results in critical paths with richer error info.
- [ ] Preserve and expose `errno` + operation context.
- [ ] Keep API ergonomics simple (either exceptions with context, or result type).

## Next (important soon after MVP)

### 6 Address and bind usability

- [ ] Add resolver-based bind/connect path (`getaddrinfo`) to avoid manual address handling.
- [ ] Keep typed IPv4/IPv6 helpers, but make resolver the default user path.
- [ ] Expose `getsockname`/`getpeername` helpers for diagnostics/logging.

### 7 Non-blocking and concurrency support

- [ ] Add non-blocking mode toggle.
- [ ] Add `poll`-based readiness helper first (portable and enough for initial versions).
- [ ] Leave `epoll` as a later backend unless performance requires it.

### 8 API cleanup for maintainability

- [ ] Unify naming and behavior across client/server classes.
- [ ] Add native handle access (`native_handle`) for integration with custom loops.
- [ ] Keep public surface small; avoid adding advanced syscalls too early.

## Explicitly deferred (not needed for HTTP/1.0 now)

- [ ] UDP-specific send/receive APIs (`sendto`, `recvfrom`) unless you add DNS or other UDP features.
- [ ] Multicast/broadcast controls.
- [ ] Unix-domain advanced features (FD passing, credentials).
- [ ] Raw sockets and `SEQPACKET` support.
- [ ] `sendmsg`/`recvmsg`, `readv`/`writev`, `recvmmsg`/`sendmmsg`.
- [ ] `socketpair`, `dup*`, and other low-level FD utilities.

## Milestones

### Milestone A — “Can serve simple HTTP/1.0”

- [ ] Bind/listen/accept works.
- [ ] `recv` request bytes.
- [ ] `send` response bytes.
- [ ] Graceful close/shutdown.
- [ ] Basic reusable bind (`SO_REUSEADDR`).

### Milestone B — “Can handle real traffic patterns”

- [ ] Better error reporting.
- [ ] Resolver-based bind/connect.
- [ ] Non-blocking + `poll` support.
- [ ] Optional tuning options (timeouts, nodelay, reuseport).

### Milestone C — “Scale/feature extensions (only if needed)”

- [ ] `epoll` backend.
- [ ] Extra socket families/protocols.
- [ ] Advanced Unix features.
