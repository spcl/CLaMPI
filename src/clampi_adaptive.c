#define _ISOC11_SOURCE
#include "clampi_internal.h"

#if CL_ADAPTIVE
int cl_adapt(CMPI_Win win, int reset){
    cl_cache_t * cache = (cl_cache_t *) win.cache;

    //printf("adapt check: %i %i %i %lf\n",cache->stat.last_eviction_scan, cache->stat.tot_eviction_sample, cache->stat.tot_eviction_scan, ((double) cache->stat.tot_eviction_sample)/cache->stat.tot_eviction_scan);


    //printf("adapt check: %i %i %lf\n",cache->stat.ht_evictions, cache->getcount, ((double) cache->stat.ht_evictions)/cache->getcount);
    
#ifdef CL_STATS
    int toflush=1;
    int newval_ht, newval_mem;
    if (((double) cache->stat.ht_evictions)/cache->getcount > CL_HT_INC_THRESHOLD){    
        newval_ht = ((double) CL_HT_ENTRIES(cache)) * ((double) CL_HT_INC);

        CLPRINT("**** AUTOFLUSH CL_HT_INC: %lf; (increasing ht size; ht_evictions: %i; getcount: %i; current ht size: %i; new ht size: %i) ***\n", CL_HT_INC, cache->stat.ht_evictions, cache->getcount, CL_HT_ENTRIES(cache), newval_ht);

    }else if (cache->stat.last_eviction_scan>0 && ((double) cache->stat.tot_eviction_sample/cache->stat.tot_eviction_scan) < CL_HT_DEC_THRESHOLD){
        newval_ht = (CL_HT_ENTRIES(cache)) * (CL_HT_DEC);

        CLPRINT("**** AUTOFLUSH CL_HT_DEC: %lf; (decreasing ht size; last_eviction_scan: %i; last_eviction_sample: %i; current ht size: %i; new ht size: %i) ***\n", CL_HT_DEC, cache->stat.tot_eviction_scan, cache->stat.tot_eviction_sample, CL_HT_ENTRIES(cache), newval_ht);

    }else if (((double) cache->stat.sp_evictions)/cache->getcount > CL_SP_INC_THRESHOLD){
        newval_mem = cache->mem_size * CL_SP_INC;

        CLPRINT("**** AUTOFLUSH (increasing mem size; sp_evictions: %i; getcount: %i; new mem size: %i) ***\n", cache->stat.sp_evictions, cache->getcount, newval_mem);

        toflush = 2;
    }else toflush=0;

    if (toflush>0) {
        if (cache->stat_file!=NULL) cl_print_stats(cache->stat_file, win);
        cl_print_stats(stdout, win);
    }

    if (reset) {
	    newval_ht = clampi.htsize;
	    newval_mem = clampi.memsize;
    }
  
    if (toflush==1 || reset){
        cache->ht_entries = newval_ht;
	    //printf("autoflush: new htsize: %i (%i)\n", cache->ht_entries, newval);
        free(cache->table_ptr);
        free(cache->free_entries);
        free(cache->table);
        free(cache->free_idx);
        ALLOC(CL_HT_ENTRIES(cache)*2*sizeof(cl_entry_t), void, TABLEPTR(cache));
        ALLOC(CL_HT_ENTRIES(cache)*sizeof(cl_index_t), cl_index_t, cache->table);
        ALLOC(CL_HT_ENTRIES(cache)*2*sizeof(uint32_t), uint32_t, cache->free_entries);
        cache->free_idx = Tree_New(free_entry_comp, free_entry_print, free_entry_replace, cache, CL_HT_ENTRIES(cache));
    }

    if (toflush==2 || reset){
        cache->mem_size = CLALIGN(newval_mem);
        free(MEMORY(cache));
        ALLOC(cache->mem_size, void, MEMORY(cache));
    }
    
    if (toflush>0 || reset) CMPI_Win_invalidate(win);

    return toflush;
#else
    printf("Warning: you enabled CL_ADAPTIVE without CL_STATS: CL_ADAPTIVE is ignored!\n");
#endif
    return 0;
}
#endif


int cl_reset(CMPI_Win win){
#ifdef CL_ADAPTIVE
    return cl_adapt(win, 1);
#else
    return -1;
#endif
}
