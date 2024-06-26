#include "bitwise.h"

u64 ceil_two_pow(u64 num) {
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    num |= num >> 32;
    return num + 1;
}

void* offset(void* ptr, i64 offset) {
    u64 a = (u64)ptr;
    a += offset;
    return (void*)a;
}

static u64 swap_bytes(u64 x) {
    u8* array = (u8*)&x;

    for (i64 i = 0; i < 4; i++) {
        u8 tmp = array[i];
        array[i] = array[7 - i];
        array[7 - i] = tmp;
    }
    return x;
}

u64 hton64(u64 x) {
#ifdef __LITTLE_ENDIAN__
    return swap_bytes(x);
#elif defined(__BIG_ENDIAN__)
    return x;
#else
    int n = 1;
    if (((char*)&n)[0] == 1)
        return swap_bytes(x);
    else
        return x;
#endif
}

u64 ntoh64(u64 x) {
#ifdef __LITTLE_ENDIAN__
    return swap_bytes(x);
#elif defined(__BIG_ENDIAN__)
    return x;
#else
    int n = 1;
    if (((char*)&n)[0] == 1)
        return swap_bytes(x);
    else
        return x;
#endif
}
