# tlsf-malloc

Two-Level Segregated Fit memory allocator C++ implementation.

This implementation is based on https://github.com/mattconte/tlsf and has the following features:
- Compatible with 32 / 64 bit platforms (but does not support blocks over 4GB)
- The allocated address must be aligned according to the `align_size` specified by the user

**Why use C++ ?**

The reason is that templates can be used to generate code at compile time for different functions, such as aligning sizes. If it is done at runtime, then it is less efficient.
