This is a simple arbitrary precision arithmetic library for operations over the
integers. It implements types for naturals and signed integers. It's not
intended to compete in terms of performance with more serious efforts like the
GNU Multiple Precision Arithmetic Library. Its main advantages are:

1. It's small. Its header and source file in at 23 kilobytes, at the time of
   this writing.
2. It's easy to integrate, since it's only two files.

It can be useful for quick tests, or for small projects that need to handle
large numbers but don't need the power of bigger libraries.
