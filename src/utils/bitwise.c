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

void* offsetu(void* ptr, u64 offset) {
    u64 a = (u64)ptr;
    a += offset;
    return (void*)a;
}

static void swap_bytes(void* x, u64 size) {
    u8* array = x;

    for (u64 lo = 0, hi = size - 1; lo < size; lo++, hi--) {
        u8 tmp = array[lo];
        array[lo] = array[hi];
        array[hi] = tmp;
    }
}

u64 hton64(u64 x) {
#if defined __LITTLE_ENDIAN__ || defined __ORDER_LITTLE_ENDIAN__
    swap_bytes(&x, sizeof x);
#elif defined __ORDER_LITTLE_ENDIAN__ || defined __BIG_ENDIAN__
    int n = 1;
    if (((u8*)&n)[0] == 1)
        swap_bytes(&x, sizeof x);
#endif
    return x;
}

u64 ntoh64(u64 x) {
#if defined __LITTLE_ENDIAN__ || defined __ORDER_LITTLE_ENDIAN__
    swap_bytes(&x, sizeof x);
#elif defined __ORDER_LITTLE_ENDIAN__ || defined __BIG_ENDIAN__
    int n = 1;
    if (((u8*)&n)[0] == 1)
        swap_bytes(&x, sizeof x);
#endif
    return x;
}

i16 big_endian16(i16 x) {
#if defined __LITTLE_ENDIAN__ || defined __ORDER_LITTLE_ENDIAN__
    swap_bytes(&x, sizeof x);
#elif defined __ORDER_LITTLE_ENDIAN__ || defined __BIG_ENDIAN__
    int n = 1;
    if (((u8*)&n)[0] == 1)
        swap_bytes(&x, sizeof x);
#endif
    return x;

}
i32 big_endian32(i32 x) {
#if defined __LITTLE_ENDIAN__ || defined __ORDER_LITTLE_ENDIAN__
    swap_bytes(&x, sizeof x);
#elif defined __ORDER_LITTLE_ENDIAN__ || defined __BIG_ENDIAN__
    int n = 1;
    if (((u8*)&n)[0] == 1)
        swap_bytes(&x, sizeof x);
#endif
    return x;
}
i64 big_endian64(i64 x) {
#if defined __LITTLE_ENDIAN__ || defined __ORDER_LITTLE_ENDIAN__
    swap_bytes(&x, sizeof x);
#elif defined __ORDER_LITTLE_ENDIAN__ || defined __BIG_ENDIAN__
    int n = 1;
    if (((u8*)&n)[0] == 1)
        swap_bytes(&x, sizeof x);
#endif
    return x;
}
