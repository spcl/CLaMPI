#ifndef __CLAMPI_UTILS__
#define __CLAMPI_UTILS__
#include <stdint.h>
#include <stdlib.h>
#include "clampi_internal.h"

void cl_get_abs_victim(CMPI_Win win, double * minscore, double * maxscore, double * avgscore, int * ivictim, double ref, int * lt);

uint32_t fast_srand(uint64_t seed);
uint32_t fast_rand();

void sse_memcpy(char *to, const char *from, size_t len);
void print_neigh(cl_cache_t * cache, cl_entry_t * newe);

#endif /* __CLAMPI_UTILS__ */
