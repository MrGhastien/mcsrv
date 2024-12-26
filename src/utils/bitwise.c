#include "bitwise.h"

#if defined __LITTLE_ENDIAN__ || defined __ORDER_LITTLE_ENDIAN__
#define CONVERT_ENDIANNESS(x)                                                                      \
    swap_bytes(&(x), sizeof(x));                                                                   \
    return x
#elif defined __ORDER_LITTLE_ENDIAN__ || defined __BIG_ENDIAN__
#define CONVERT_ENDIANNESS(x) return (x)
#else
#define CONVERT_ENDIANNESS(x)                                                                      \
    int n = 1;                                                                                     \
    if (((u8*) &n)[0] == 1) {                                                                      \
        swap_bytes(&(x), sizeof(x));                                                               \
    }                                                                                              \
    return x
#endif

u64 ceil_two_pow(u64 num) {
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    num |= num >> 32;
    return num + 1;
}

void* offset(const void* ptr, i64 offset) {
    u64 a = (u64) ptr;
    a += offset;
    return (void*) a;
}

void* offsetu(const void* ptr, u64 offset) {
    u64 a = (u64) ptr;
    a += offset;
    return (void*) a;
}

static void swap_bytes(void* x, u64 size) {
    u8* array = x;

    for (u64 lo = 0, hi = size - 1; lo < (size >> 1); lo++, hi--) {
        u8 tmp = array[lo];
        array[lo] = array[hi];
        array[hi] = tmp;
    }
}

NTOH(64) {
    CONVERT_ENDIANNESS(x);
}

NTOH(32) {
    CONVERT_ENDIANNESS(x);

}

NTOH(16) {
    CONVERT_ENDIANNESS(x);
}

UNTOH(64) {
    CONVERT_ENDIANNESS(x);
}

UNTOH(32) {
    CONVERT_ENDIANNESS(x);
}

UNTOH(16) {
    CONVERT_ENDIANNESS(x);
}


HTON(64) {
    CONVERT_ENDIANNESS(x);
}

HTON(32) {
    CONVERT_ENDIANNESS(x);
}

HTON(16) {
    CONVERT_ENDIANNESS(x);
}

UHTON(64) {
    CONVERT_ENDIANNESS(x);
}

UHTON(32) {
    CONVERT_ENDIANNESS(x);
}

UHTON(16) {
    CONVERT_ENDIANNESS(x);
}
