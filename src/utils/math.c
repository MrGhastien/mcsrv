#include "math.h"

u64 min_u64(u64 a, u64 b) {
    return b < a ? b : a;
}

u64 ceil_u64(u64 a, u64 multiple) {
    if(multiple == 0)
        return a;

    u64 remainder = a % multiple;
    if(remainder == 0)
        return a;

    return a + multiple - remainder;
}

i64 ceil_i64(i64 a, i64 multiple) {

    multiple = abs_i64(multiple);
    
    if(multiple == 0)
        return a;

    u64 remainder = a % multiple;
    if(remainder == 0)
        return a;

    return a + multiple - remainder;
}

i64 abs_i64(i64 x) {
    return x < 0 ? -x : x;
}

u64 sub_no_underflow(u64 a, u64 b) {
    if(b > a)
        return 0;

    return a - b;
}

u64 clamp(u64 x, u64 inf, u64 sup) {
    if(x < inf)
        return inf;
    if(x > sup)
        return sup;
    return x;
}
