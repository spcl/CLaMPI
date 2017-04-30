#include <string.h>
#include "utils.h"
#include "clampi_internal.h"

void cl_get_abs_victim(CMPI_Win win, double * minscore, double * maxscore, double * avgscore, int * ivictim, double ref, int * lt){

    cl_cache_t * cache = (cl_cache_t *) win.cache;

    double cscore;
    double sum = 0;

    *lt = 0;
    *minscore=0;

    int i, c=0;
    for (i=0; i<CL_HT_ENTRIES(cache); i++){
            
            
        if (cache->table[i].lasthit>0){

            cscore = SCORE(cache->table[i].lasthit, cache->table[i].penalty);
            if (cscore<=*minscore || c==0) { 
                *minscore=cscore;
                *ivictim=cache->table[i].index;

            }

            if (cscore>*maxscore || c==0) *maxscore = cscore;

            if (cscore < ref) (*lt)++;
            sum += cscore;
            c++;
        }
    }
    *avgscore = sum/c;
}


#ifdef CL_SSEMEMCPY
void sse_memcpy(char *to, const char *from, size_t len){
 unsigned long src = (unsigned long)from;
 unsigned long dst = (unsigned long)to;
 int i;

 /* check alignment */
 if ((src ^ dst) & 0xf)
   goto unaligned;

 if (src & 0xf) {
   int chunk = 0x10 - (src & 0xf);
   if (chunk > len) {
     chunk = len; /* chunk can be more than the actual amount to copy */
   }

   /* copy chunk until next 16-byte  */
   memcpy(to, from, chunk);
   len -= chunk;
   to += chunk;
   from += chunk;
 }

 /*
  * copy in 256 Byte portions
  */
 for (i = 0; i < (len & ~0xff); i += 256) {
   __asm__ volatile(
   "movaps 0x0(%0),  %%xmm0\n\t"
   "movaps 0x10(%0), %%xmm1\n\t"
   "movaps 0x20(%0), %%xmm2\n\t"
   "movaps 0x30(%0), %%xmm3\n\t"
   "movaps 0x40(%0), %%xmm4\n\t"
   "movaps 0x50(%0), %%xmm5\n\t"
   "movaps 0x60(%0), %%xmm6\n\t"
   "movaps 0x70(%0), %%xmm7\n\t"
   "movaps 0x80(%0), %%xmm8\n\t"
   "movaps 0x90(%0), %%xmm9\n\t"
   "movaps 0xa0(%0), %%xmm10\n\t"
   "movaps 0xb0(%0), %%xmm11\n\t"
   "movaps 0xc0(%0), %%xmm12\n\t"
   "movaps 0xd0(%0), %%xmm13\n\t"
   "movaps 0xe0(%0), %%xmm14\n\t"
   "movaps 0xf0(%0), %%xmm15\n\t"

   "movaps %%xmm0,  0x0(%1)\n\t"
   "movaps %%xmm1,  0x10(%1)\n\t"
   "movaps %%xmm2,  0x20(%1)\n\t"
   "movaps %%xmm3,  0x30(%1)\n\t"
   "movaps %%xmm4,  0x40(%1)\n\t"
   "movaps %%xmm5,  0x50(%1)\n\t"
   "movaps %%xmm6,  0x60(%1)\n\t"
   "movaps %%xmm7,  0x70(%1)\n\t"
   "movaps %%xmm8,  0x80(%1)\n\t"
   "movaps %%xmm9,  0x90(%1)\n\t"
   "movaps %%xmm10, 0xa0(%1)\n\t"
   "movaps %%xmm11, 0xb0(%1)\n\t"
   "movaps %%xmm12, 0xc0(%1)\n\t"
   "movaps %%xmm13, 0xd0(%1)\n\t"
   "movaps %%xmm14, 0xe0(%1)\n\t"
   "movaps %%xmm15, 0xf0(%1)\n\t"
   : : "r" (from), "r" (to) : "memory");

   from += 256;
   to += 256;
 }

 goto trailer;

unaligned:
 /*
  * copy in 256 Byte portions unaligned
  */
 for (i = 0; i < (len & ~0xff); i += 256) {
   __asm__ volatile(
   "movups 0x0(%0),  %%xmm0\n\t"
   "movups 0x10(%0), %%xmm1\n\t"
   "movups 0x20(%0), %%xmm2\n\t"
   "movups 0x30(%0), %%xmm3\n\t"
   "movups 0x40(%0), %%xmm4\n\t"
   "movups 0x50(%0), %%xmm5\n\t"
   "movups 0x60(%0), %%xmm6\n\t"
   "movups 0x70(%0), %%xmm7\n\t"
   "movups 0x80(%0), %%xmm8\n\t"
   "movups 0x90(%0), %%xmm9\n\t"
   "movups 0xa0(%0), %%xmm10\n\t"
   "movups 0xb0(%0), %%xmm11\n\t"
   "movups 0xc0(%0), %%xmm12\n\t"
   "movups 0xd0(%0), %%xmm13\n\t"
   "movups 0xe0(%0), %%xmm14\n\t"
   "movups 0xf0(%0), %%xmm15\n\t"

   "movups %%xmm0,  0x0(%1)\n\t"
   "movups %%xmm1,  0x10(%1)\n\t"
   "movups %%xmm2,  0x20(%1)\n\t"
   "movups %%xmm3,  0x30(%1)\n\t"
   "movups %%xmm4,  0x40(%1)\n\t"
   "movups %%xmm5,  0x50(%1)\n\t"
   "movups %%xmm6,  0x60(%1)\n\t"
   "movups %%xmm7,  0x70(%1)\n\t"
   "movups %%xmm8,  0x80(%1)\n\t"
   "movups %%xmm9,  0x90(%1)\n\t"
   "movups %%xmm10, 0xa0(%1)\n\t"
   "movups %%xmm11, 0xb0(%1)\n\t"
   "movups %%xmm12, 0xc0(%1)\n\t"
   "movups %%xmm13, 0xd0(%1)\n\t"
   "movups %%xmm14, 0xe0(%1)\n\t"
   "movups %%xmm15, 0xf0(%1)\n\t"
   : : "r" (from), "r" (to) : "memory");

   from += 256;
   to += 256;
 }

trailer:
 memcpy(to, from, len & 0xff);
}
#endif

#ifdef CL_FASTRAND
uint64_t g_seed;

uint32_t fast_srand(uint64_t seed){
    g_seed = seed;
}


uint32_t fast_rand(){
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16)&0x7FFF;
}
#endif



#define STAT_PRINT(NAME, TYPE, VAL) fprintf(fp, "%i %i %i %i %i %s %"#TYPE"\n", rank, CL_MAX_SCAN, CL_HT_ENTRIES(cache), CL_MEM_SIZE(cache), stats.total, NAME, VAL); 
void cl_print_stats(FILE * fp, CMPI_Win win){
    cl_stats_full stats;
    cl_get_stats(win, &stats);

    cl_cache_t * cache = (cl_cache_t *) win.cache;

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    fprintf(fp, "RANK CL_MAX_SCAN CL_HT_ENTRIES CL_MEM_SIZE GETCOUNT NAME VALUE\n");
    STAT_PRINT("HITS", u, stats.base.hits);
    STAT_PRINT("FAILED", u, stats.base.failed);                                                   
    STAT_PRINT("HT_EVICTIONS", u, stats.base.ht_evictions);                                      
    STAT_PRINT("SP_EVICTIONS", u, stats.base.sp_evictions);                                  
    STAT_PRINT("CACHED", u, stats.base.cached);
    STAT_PRINT("LOCALS", u, stats.base.locals);
    STAT_PRINT("ENTRIES", i, stats.entries);
    STAT_PRINT("FREE_REGIONS", i, stats.free_regions);
    STAT_PRINT("LOAD_FACTOR", lf, stats.load_factor); 
    STAT_PRINT("MMS", lf, stats.mms); 
    STAT_PRINT("TOTAL_DATA", lu, stats.base.total_data);   
}

int cl_get_stats(CMPI_Win win, cl_stats_full * stats){
#ifdef CL_STATS
    cl_cache_t * cache = (cl_cache_t *) win.cache;
    memcpy(&(stats->base), &cache->stat, sizeof(cl_stats));
    stats->free_regions = Tree_CountNodes(cache->free_idx);
    stats->entries = (CL_HT_ENTRIES(cache)*2 - cache->next_free_entry) - stats->free_regions;
    stats->load_factor = (double) stats->entries / CL_HT_ENTRIES(cache);
    stats->total = cache->getcount;
    stats->mms = cache->mms;
    return CL_SUCCESS;
#else
    printf("Warning: cl_get_stats: compiled without CL_STATS!!!\n");
    return CL_ERROR;
#endif
}


void print_neigh(cl_cache_t * cache, cl_entry_t * newe){

    if (newe->mprev!=NOENTRY) printf("prev (%i): [%u %u] ", TABLEENTRY(cache, newe->mprev)->type, CLEFT(TABLEENTRY(cache, newe->mprev)), CRIGHT(TABLEENTRY(cache, newe->mprev)));
    else printf ("prev: NULL ");

    printf("entry (%i): [%u %u]; ", newe->type, CLEFT(newe), CRIGHT(newe));
    
    if (newe->mnext!=NOENTRY) printf("next (%i): [%u %u] ", TABLEENTRY(cache, newe->mnext)->type, CLEFT(TABLEENTRY(cache, newe->mnext)), CRIGHT(TABLEENTRY(cache, newe->mnext)));
    else printf ("next: NULL ");

}
