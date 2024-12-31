#ifndef MATH_H
#define MATH_H

#include "definitions.h"

u64 min_u64(u64 a, u64 b);

u64 ceil_u64(u64 a, u64 multiple);
i64 ceil_i64(i64 a, i64 multiple);

i64 abs_i64(i64 x);

u64 sub_no_underflow(u64 a, u64 b);
u64 clamp(u64 x, u64 inf, u64 sup);

#endif /* ! MATH_H */
