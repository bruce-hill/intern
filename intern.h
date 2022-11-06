#pragma once
// Simple API for interning values.
// Interned values are garbage collected using the Boehm garbage collector (not
// immortal), but the most recently interned items are kept in memory to
// prevent churn.

typedef const char* istr_t;

// Intern a chunk of bytes which may or may not contain references to GC objects
const void *intern_bytes(const void *bytes, size_t len);
// Intern a NUL-terminated string
istr_t intern_str(const char *str);
// Intern a length-delimited string (automatically appending a terminating NUL byte)
// (Useful for interning a slice of a string)
istr_t intern_strn(const char *str, size_t len);
// Get the size of an interned object (faster than strlen(), doesn't rely on NUL termination)
size_t intern_len(const char *str);
// Randomize the hash function used for interning
void randomize_hash(void);
