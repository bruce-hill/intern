// Code for interning chunks of data in a way compatible with the Boehm garbage collector
#include <assert.h>
#include <gc.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>

#include "intern.h"

typedef struct intern_entry_s {
    char *mem;
    size_t len;
    struct intern_entry_s *next;
} intern_entry_t;

static intern_entry_t *interned = NULL, *lastfree = NULL;
static size_t intern_capacity = 0, intern_count = 0;

// Cache most recently used strings to prevent GC churn
#ifndef N_RECENTLY_USED
#define N_RECENTLY_USED 256
#endif
static const char **recently_used = NULL;
static int recently_used_i = 0;

static void intern_insert(char *mem, size_t len);
static void rehash(void);

#ifdef NOSIPHASH
static size_t initial_hash = 0;
void randomize_hash(void) {
    getrandom(&initial_hash, sizeof(initial_hash), 0);
    rehash();
}

static inline size_t hash_mem(const char *mem, size_t len)
{
    if (__builtin_expect(len == 0, 0)) return 0;
    register unsigned char *p = (unsigned char *)mem;
    register size_t h = (size_t)(*p << 7) ^ len ^ initial_hash;
    register size_t i = len > 128 ? 128 : len;
    while (i--)
        h = (1000003*h) ^ *p++;
    if (h == 0) h = 1234567;
    return h;
}
#else
#include "SipHash/halfsiphash.h"
#include "SipHash/halfsiphash.c"

static uint8_t hash_random_vector[16] = {42,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
void randomize_hash(void) {
    getrandom(hash_random_vector, sizeof(hash_random_vector), 0);
    rehash();
}

static size_t hash_mem(const char *mem, size_t len)
{
    size_t hash;
    halfsiphash(mem, len, hash_random_vector, (uint8_t*)&hash, sizeof(hash));
    return hash;
}
#endif

static void rehash(void)
{
    // Calculate new size to be min(16, next_highest_power_of_2(2 * num_entries))
    size_t new_size = 0;
    for (size_t i = 0; i < intern_capacity; i++)
        if (interned[i].mem)
            new_size += 2;
    if (new_size < 16) new_size = 16;
    size_t pow2 = 1;
    while (pow2 < new_size) pow2 <<= 1;
    new_size = pow2;

    intern_entry_t *old = interned;
    size_t old_capacity = intern_capacity;
    interned = GC_MALLOC_UNCOLLECTABLE(new_size*sizeof(intern_entry_t));
    intern_capacity = new_size;
    intern_count = 0;
    lastfree = &interned[new_size - 1];
    // Rehash:
    if (old) {
        for (size_t i = 0; i < old_capacity; i++) {
            if (old[i].mem)
                GC_unregister_disappearing_link((void**)&old[i].mem);
            if (old[i].mem)
                intern_insert(GC_REVEAL_POINTER(old[i].mem), old[i].len);
        }
        GC_FREE(old);
    }
}

static char *lookup(const char *mem, size_t len)
{
    if (intern_capacity == 0 || !mem) return NULL;
    int i = (int)(hash_mem(mem, len) & (size_t)(intern_capacity-1));
    for (intern_entry_t *e = &interned[i]; e; e = e->next) {
        if (e->mem && e->len == len && memcmp(mem, GC_REVEAL_POINTER(e->mem), len) == 0)
            return GC_REVEAL_POINTER(e->mem);
    }
    return NULL;
}

static void intern_insert(char *mem, size_t len)
{
    if (!mem) return;

    // Grow the storage if necessary
    if ((intern_count + 1) >= intern_capacity)
        rehash();

    int i = (int)(hash_mem(mem, len) & (size_t)(intern_capacity-1));
    intern_entry_t *collision = &interned[i];
    if (!collision->mem) { // No collision
        collision->mem = (char*)GC_HIDE_POINTER(mem);
        assert(!GC_general_register_disappearing_link((void**)&collision->mem, mem));
        collision->len = len;
        ++intern_count;
        return;
    }

    while (lastfree >= interned && lastfree->len)
        --lastfree;

    int i2 = (int)(hash_mem(GC_REVEAL_POINTER(collision->mem), collision->len) & (size_t)(intern_capacity-1));
    if (i2 == i) { // Collision with element in its main position
        lastfree->mem = (char*)GC_HIDE_POINTER(mem);
        assert(!GC_general_register_disappearing_link((void**)&lastfree->mem, mem));
        lastfree->len = len;
        lastfree->next = collision->next;
        collision->next = lastfree;
    } else {
        intern_entry_t *prev = &interned[i2];
        while (prev->next != collision)
            prev = prev->next;
        memcpy(lastfree, collision, sizeof(intern_entry_t));
        assert(!GC_move_disappearing_link((void**)&collision->mem, (void**)&lastfree->mem));
        prev->next = lastfree;
        collision->mem = (char*)GC_HIDE_POINTER(mem);
        assert(!GC_general_register_disappearing_link((void**)&collision->mem, mem));
        collision->len = len;
        collision->next = NULL;
    }
    ++intern_count;
}

const void *intern_bytes(const void *bytes, size_t len)
{
    if (!bytes) return NULL;
    const char *intern = lookup(bytes, len);
    if (!intern) {
        // GC_MALLOC() means this may contain pointers to other stuff to keep alive in GC
        char *tmp = GC_MALLOC(sizeof(size_t) + len + 1);
        *(size_t*)tmp = len;
        tmp += sizeof(size_t);
        memcpy(tmp, bytes, len);
        tmp[len] = '\0';
        intern_insert(tmp, len);
        intern = tmp;
    }
    if (!recently_used) recently_used = GC_MALLOC(sizeof(char*)*N_RECENTLY_USED);
    recently_used[recently_used_i] = intern;
    recently_used_i = (recently_used_i + 1) & (N_RECENTLY_USED-1);
    return intern;
}

size_t intern_len(const char *str)
{
    istr_t istr = intern_str(str);
    return ((size_t*)istr)[-1];
}

istr_t intern_str(const char *str)
{
    return intern_strn(str, strlen(str));
}

istr_t intern_strn(const char *str, size_t len)
{
    if (!str) return NULL;
    istr_t intern = lookup(str, len);
    if (!intern) {
        // GC_MALLOC_ATOMIC() means this memory doesn't need to be scanned by the GC
        char *tmp = GC_MALLOC_ATOMIC(sizeof(size_t) + len + 1);
        *(size_t*)tmp = len;
        tmp += sizeof(size_t);
        memcpy(tmp, str, len);
        tmp[len] = '\0';
        intern_insert(tmp, len);
        intern = tmp;
    }
    if (!recently_used)
        recently_used = GC_MALLOC_UNCOLLECTABLE(sizeof(char*)*N_RECENTLY_USED);
    recently_used[recently_used_i] = intern;
    recently_used_i = (recently_used_i + 1) & (N_RECENTLY_USED-1);
    return intern;
}

istr_t intern_strf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char *tmp = NULL;
    int len = vasprintf(&tmp, fmt, args);
    if (len < 0) return NULL;
    va_end(args);
    istr_t ret = intern_strn(tmp, (size_t)len);
    free(tmp);
    return ret;
}
