/* Rotinas de runtime do compilador (normalmente vem do libgcc/compiler-rt do NDK) */
#include <stdint.h>
#include <stddef.h>
#define H __attribute__((visibility("hidden")))
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);

#if defined(__arm__)
/* chamadas indiretas: impedem o compilador de transformar memcpy() de volta em __aeabi_memcpy */
static void *(*volatile p_memcpy)(void *, const void *, size_t) = memcpy;
static void *(*volatile p_memmove)(void *, const void *, size_t) = memmove;
static void *(*volatile p_memset)(void *, int, size_t) = memset;
static uint32_t udiv32(uint32_t n, uint32_t d, uint32_t *rem) {
    uint32_t q = 0, r = 0;
    if (d == 0) { *rem = 0; return 0xffffffffu; }
    for (int i = 31; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1);
        if (r >= d) { r -= d; q |= 1u << i; }
    }
    *rem = r;
    return q;
}
H uint32_t __aeabi_uidiv(uint32_t n, uint32_t d) { uint32_t r; return udiv32(n, d, &r); }
H int32_t __aeabi_idiv(int32_t n, int32_t d) {
    uint32_t r, un = n < 0 ? -(uint32_t)n : (uint32_t)n, ud = d < 0 ? -(uint32_t)d : (uint32_t)d;
    uint32_t q = udiv32(un, ud, &r);
    return ((n < 0) != (d < 0)) ? -(int32_t)q : (int32_t)q;
}
/* retornam quociente em r0 e resto em r1 (par de 64 bits) */
H uint64_t __aeabi_uidivmod(uint32_t n, uint32_t d) { uint32_t r; uint32_t q = udiv32(n, d, &r); return ((uint64_t)r << 32) | q; }
H int64_t __aeabi_idivmod(int32_t n, int32_t d) {
    uint32_t r, un = n < 0 ? -(uint32_t)n : (uint32_t)n, ud = d < 0 ? -(uint32_t)d : (uint32_t)d;
    uint32_t q = udiv32(un, ud, &r);
    int32_t qs = ((n < 0) != (d < 0)) ? -(int32_t)q : (int32_t)q;
    int32_t rs = n < 0 ? -(int32_t)r : (int32_t)r;
    return (int64_t)(((uint64_t)(uint32_t)rs << 32) | (uint32_t)qs);
}
H void __aeabi_memcpy(void *d, const void *s, size_t n) { p_memcpy(d, s, n); }
H void __aeabi_memcpy4(void *d, const void *s, size_t n) { p_memcpy(d, s, n); }
H void __aeabi_memcpy8(void *d, const void *s, size_t n) { p_memcpy(d, s, n); }
H void __aeabi_memmove(void *d, const void *s, size_t n) { p_memmove(d, s, n); }
H void __aeabi_memmove4(void *d, const void *s, size_t n) { p_memmove(d, s, n); }
H void __aeabi_memmove8(void *d, const void *s, size_t n) { p_memmove(d, s, n); }
H void __aeabi_memset(void *d, size_t n, int c) { p_memset(d, c, n); }
H void __aeabi_memset4(void *d, size_t n, int c) { p_memset(d, c, n); }
H void __aeabi_memset8(void *d, size_t n, int c) { p_memset(d, c, n); }
H void __aeabi_memclr(void *d, size_t n) { p_memset(d, 0, n); }
H void __aeabi_memclr4(void *d, size_t n) { p_memset(d, 0, n); }
H void __aeabi_memclr8(void *d, size_t n) { p_memset(d, 0, n); }
#endif
