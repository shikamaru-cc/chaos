#ifndef __LIB_RAND_H
#define __LIB_RAND_H

#include "stdint.h"

void rand_set_seed(uint32_t seed);
uint32_t rand(void);

#endif
