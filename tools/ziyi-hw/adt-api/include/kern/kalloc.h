#include <stdlib.h>
#define Z_WAITOK 0
#define kalloc_type(t,f) ((t*)calloc(1,sizeof(t)))
#define kfree_type(t,p) free(p)
