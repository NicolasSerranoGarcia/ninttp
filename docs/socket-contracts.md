# Socket contracts

This project is not trying to abstract every networking API in existence. The
socket backend layer exists to hide the practical differences between two
families:

- POSIX sockets on Unix-like systems.
- Winsock on Windows.

Those APIs are similar enough that one backend contract is easier to understand
and maintain than a pile of tiny capability concepts. The project should stay
boring here. The HTTP layer is where higher-level abstractions will grow.

## Design rule

Keep the backend abstraction concrete and platform-oriented. Keep protocol and
wrapper types relaxed.

In practice this means:

- `SocketBackend` is one structural concept for the selected platform backend.
- `ListenerSocket`, `StreamSocket`, endpoints, and HTTP wrappers should avoid
  broad class-level constraints when a member function can naturally require
  only what it uses.
- Semantic expectations are documented here instead of being pretended into
  concepts. Concepts can check spelling and return types; they cannot check
  resource ownership, whether an error code was captured at the right time, or
  whether a socket is truly usable.

## Backend contract

A backend is expected to look like a small normalized wrapper around POSIX or
Winsock.

Required types:

- `SocketT`: the native socket handle type. POSIX uses an integer descriptor;
  Winsock uses `SOCKET`.
- `ErrorT`: the native error-code type.
- `AddressStorageT`: storage for a native socket address. POSIX will usually use
  `sockaddr_storage`; Winsock has compatible address storage.
- `AddressLenT`: the length type used by address operations. POSIX usually uses
  `socklen_t`; Winsock commonly uses `int`.
- `AddressBundleT`: a simple aggregate returned by `accept()`.

`AddressBundleT` must have these fields:

- `socket`, with type `SocketT`.
- `storage`, with type `AddressStorageT`.
- `len`, with type `AddressLenT`.

Required functions:

- `invalidSocket() -> SocketT`.
- `openSocket(Domain, Service, Protocol) -> SocketT`.
- `closeSocket(const SocketT&) -> bool`.
- `isValidSocket(const SocketT&) -> bool`.
- `shutdownSocket(const SocketT&, ShutdownPolicy) -> bool`.
- `bind(const SocketT&, const AddressStorageT*, AddressLenT) -> bool`.
- `listen(const SocketT&, int) -> bool`.
- `accept(const SocketT&) -> std::optional<AddressBundleT>`.
- `connect(const SocketT&, const AddressStorageT*, AddressLenT) -> bool`.
- `send(const SocketT&, const char*, std::size_t) -> signed byte count`.
- `receive(const SocketT&, char*, std::size_t) -> signed byte count`.
- `getLastError() -> ErrorT`.
- `getMsgFromError(ErrorT) -> std::string`.

If `NINTTP_SOCKET_BACKEND_REQUIRES_INIT == 1`, the backend must also provide:

- `init() -> bool`.
- `deinit() -> bool`.

## Backend semantics

Backend functions should not throw to report ordinary socket failures. They
should return the failure value listed in the contract and make the native error
available through `getLastError()`.

Boolean operations return `true` on success and `false` on failure.

`openSocket()` returns a valid `SocketT` on success and the backend's invalid
socket value on failure.

`accept()` returns an `AddressBundleT` on success and `std::nullopt` on failure.
The accepted socket in the bundle is owned by the caller after a successful
return.

`send()` and `receive()` return a non-negative byte count on success. A negative
value means failure. A zero-byte receive keeps the normal native meaning: the
peer closed the stream cleanly.

`getLastError()` should capture the backend's current socket error immediately
after the failed native call. On POSIX this is normally `errno`. On Winsock this
is normally `WSAGetLastError()`.

## SocketT requirements

`SocketCore` owns `SocketT`, so `SocketT` must be:

- Move constructible.
- Move assignable.
- Swappable.

`nativeHandle()` returns a `const SocketT&`. Code that merely needs to inspect or
pass the handle can use that. Code that wants to take ownership must call
`release()`.

## Endpoint contracts

Endpoints are intentionally lighter than backends. They are value objects that
describe protocol-level address data. They must not store native backend address
storage or include platform socket headers.

`Ipv4Endpoint` stores:

- IPv4 address in host byte order.
- Port in host byte order.

The selected backend owns translation between endpoint values and native address
storage:

- `SelectedBackend::toStorage(endpoint)`.
- `SelectedBackend::storageLen(endpoint)`.
- `SelectedBackend::fromStorage<EndpointT>(storage)`.

That separation is deliberate. POSIX can use `sockaddr_in`; Winsock can use its
own native address structs; `Ipv4Endpoint` does not care.

These are still method-level contracts. If a future HTTP type only wraps an
already connected stream, it should not need to satisfy endpoint conversion rules
it never uses.

## Accepted socket contract

`ListenerSocket<EndpointT, ConnectedSocketT>::accept()` calls:

```cpp
ConnectedSocketT::fromAccepted(socket, domain, service, protocol, peerStorage)
```

That function must return something convertible to:

```cpp
std::expected<ConnectedSocketT, socketError>
```

The accepted native socket is passed into `fromAccepted()`. Treat that as an
ownership transfer into the connected socket wrapper.

## Why not more backend concepts?

There are other socket-like stacks: embedded RTOS APIs, user-space TCP stacks,
test doubles, async runtimes, and specialized platform APIs. This library does
not need to optimize its internal shape for those yet.

If one of those becomes real, prefer writing an adapter that looks like the
contract above before splitting the contract. Split the backend concept only when
the project genuinely supports a backend that cannot reasonably model the
POSIX/Winsock shape.

Until then, one backend concept is the simpler and more honest abstraction.
