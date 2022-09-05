#pragma once
// Simple API for interning values.
// Interned values are garbage collected using the Boehm garbage collector (not
// immortal), but the most recently interned items are kept in memory to
// prevent churn.

// Intern a chunk of bytes which may or may not contain references to GC objects
const void *intern_bytes(const char *bytes, size_t len);
// Intern a NUL-terminated string
const char *intern_str(const char *str);
// Intern a length-delimited string (automatically appending a terminating NUL byte)
// (Useful for interning a slice of a string)
const char *intern_strn(const char *str, size_t len);
// Randomize the hash function used for interning
void randomize_hash(void);
