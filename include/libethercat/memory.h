#ifndef LIBETHERCAT_MEMORY_H
#define LIBETHERCAT_MEMORY_H

#include <stdint.h>
#include <stddef.h>

void *ec_malloc(size_t size);
void ec_free(void *ptr);



#endif /* LIBETHERCAT_MEMORY_H */

