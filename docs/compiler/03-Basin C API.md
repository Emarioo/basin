The basin C API is found in `include/basin/basin.h`.

What we value in the API is:
- A single header which doesn't include any other headers
- Doesn't use new C features
- Uses normal C strings
- Clear description of who owns memory, what to cleanup and when to do it.
- Zeroing enums and structs provides default behaviour.
- User specified allocator
- User specified file system

This doesn't mean that we can't use new C features internally. It's just the API that needs to be accessible.
