# User-Mode Hello Example

This self-contained example demonstrates the complete path from a packaged user
module to A32 USR mode and a custom UART syscall. Its kernel, user program,
shared example ABI, image manifest, and test all live in this directory so none
of its interfaces become part of the canonical template.

From the repository root:

```sh
make user-mode-hello-image
make -C examples/user-mode-hello test
```

The loader uses bounded static storage for up to 16 user image pages. The
example syscall writes one byte to UART0; Assignment 2 replaces it with the
required `ioctl` interface.
