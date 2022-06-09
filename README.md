# Intern

A minimalist string (or arbitrary data) interning library meant to work with
the Boehm garbage collector.

## API

```c
// Intern a chunk of bytes which may or may not contain references to GC objects
const void *intern_bytes(const char *bytes, size_t len);
// Intern a NUL-terminated string
const char *intern_str(const char *str);
// Intern a length-delimited string (automatically appending a terminating NUL byte)
// (Useful for interning a slice of a string)
const char *intern_strn(const char *str, size_t len);
```

## Usage

Here's a simple example usage:

```c
const char *str1 = intern_str("hello");
const char *str2 = intern_str("hello");
assert(str1 == str2);
const char *str3 = intern_strn("hello world", 5);
assert(str3 == str1);
```

## License

This library is released under the MIT license with the commons clause. See
[the LICENSE file](LICENSE) for full details.
