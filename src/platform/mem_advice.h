#ifndef MEM_ADVICE_H
#define MEM_ADVICE_H

enum MemoryAdvice {
    MEM_ADVICE_NONE = 0,
    MEM_ADVICE_SEQUENTIAL = 1,
    MEM_ADVICE_RANDOM = 3,
    MEM_ADVICE_UNUSED = 5,
};    

#endif /* ! MEM_ADVICE_H */
