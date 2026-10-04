# Verona echo server

A single-process TCP echo server that exercises the `socket`, `setsockopt`,
`bind`, `listen`, `accept`, `read`, `write`, and `close` bindings from the
adjacent libc project.

It listens on `127.0.0.1:35000`, accepts one client at a time, and echoes data
until that client disconnects. Its startup message uses a Verona string
literal. It currently transfers one byte per system call
because Verona does not yet expose a same-width `ssize_t` to `size_t`
conversion. Its local `bind-ipv4` function converts the loopback string and
selects the target's `sockaddr_in` layout, so this project no longer encodes
the listening address as raw bytes. Socket-option and peer-length values are
function-local addressable bindings rather than global state.

Build and run it from this directory:

```sh
verona build echo-server ./dist
./dist/echo-server
```

In another terminal, send a message:

```sh
printf 'hello\n' | nc -w 2 127.0.0.1 35000
```
