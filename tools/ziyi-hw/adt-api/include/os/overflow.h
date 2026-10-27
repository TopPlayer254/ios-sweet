#include <stdint.h>
#include <stdbool.h>
static inline bool test_add3(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t*out){uintptr_t x;return __builtin_add_overflow(a,b,&x)||__builtin_add_overflow(x,c,out);}
#define os_add3_overflow(a,b,c,o) test_add3((uintptr_t)(a),(uintptr_t)(b),(uintptr_t)(c),(uintptr_t*)(o))
