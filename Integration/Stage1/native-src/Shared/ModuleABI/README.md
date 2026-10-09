# Entropy Module ABI v1

The DLL boundary is a versioned C ABI. It exports one symbol,
`entropy_module_get_api`, which returns a function table. No Qt, JUCE, STL,
exceptions, allocators or C++ objects cross the boundary. A module allocates its
own state in `create` and frees it in `destroy`; the host owns all audio buffers.

`process_f64` receives planar FP64 buffers and must not allocate, lock, log or
touch UI/device state. Configuration is published outside the process call.
Module-specific schemas are layered on `EntropyConfigBlob` without changing the
common ABI.

