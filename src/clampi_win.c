#define _ISOC11_SOURCE

#include <string.h>
#include "clampi_internal.h"
#include "utils.h"

int _cl_win_init(CMPI_Win * win, MPI_Comm comm, MPI_Info info);


int free_entry_comp(void * a, void * b){
    uint64_t va = ((cl_entry_t *)a)->size;
    uint64_t vb = ((cl_entry_t *)b)->size;
    return (va > vb) - (va < vb);
}

void free_entry_print(void * context, void * a){
    cl_cache_t * cache = (cl_cache_t *) context;
    cl_entry_t * e = (cl_entry_t *) a;
#ifdef CLDEBUG
    printf("entry: ");
    print_neigh(cache, e);
    printf(" size: %lu; type: %i; index: %i; tnode: %p", e->size, e->type, e->index, e->tnode);
#endif
}

void free_entry_replace(void * target, void * source){
    cl_entry_t * ct = (cl_entry_t *) target;
    cl_entry_t * cs = (cl_entry_t *) source;

    //printf("Replacing!\n");
    cs->tnode = ct->tnode;
}

int CMPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm, void *baseptr, CMPI_Win * win){
    MMPI_WIN_ALLOCATE(size, disp_unit, info, comm, baseptr, &win->win);
    //printf("allocated win: %p ->: %p\n", &win->win, win->win);

    return _cl_win_init(win, comm, info);
}

int CMPI_Win_create(void * base, MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm, CMPI_Win * win){
    //printf("clampi: mpi_win_create\n");
    MMPI_WIN_CREATE(base, size, disp_unit, info, comm, &win->win);
    //printf("clampi: mpi_win_create ok\n");
    return _cl_win_init(win, comm, info);

}

int _cl_win_init(CMPI_Win * win, MPI_Comm comm, MPI_Info info){
    cl_cache_t * cache;

    if (!clampi.init){
        cl_init();
    }
    /* cache descriptor */
    ALLOC(sizeof(cl_cache_t), cl_cache_t, win->cache);
    cache = (cl_cache_t *) win->cache;

    /* set win mode */
    char mode;
    int flag=0;
    if (info!=MPI_INFO_NULL) MPI_Info_get(info, CLAMPI_MODE, 1, &mode, &flag);
    if (!flag || (mode!=CLAMPI_ALWAYS_CACHE && mode!=CLAMPI_USER_DEFINED)) mode = CLAMPI_TRANSPARENT;
    cache->mode = mode;

    /* number of ht entries */
    cache->ht_entries = clampi.htsize;

    /* memory size */
    cache->mem_size = CLALIGN(clampi.memsize);

    /* allocating memory */
    ALLOC(cache->mem_size, void, MEMORY(cache));

    /* allocating cache entry descriptors */
    /* Note: here we need htsize*2 entries because we can have
       cache entries interleaved with empty regions in the worst
       case */
    ALLOC(cache->ht_entries*2*sizeof(cl_entry_t), void, TABLEPTR(cache));

    /* allocating array of free entries (used as stack) */
    ALLOC(cache->ht_entries*2*sizeof(uint32_t), void, cache->free_entries);

    /* allocating HT */
    ALLOC(cache->ht_entries*sizeof(cl_index_t), cl_index_t, cache->table);

    /* allocating the AVL tree for the free regions */
    cache->free_idx = Tree_New(free_entry_comp, free_entry_print, free_entry_replace, cache, cache->ht_entries);

#ifdef CL_ADAPTIVE
    cache->stat_file = NULL;
#endif

    reset_data_structures(cache);

    CLPRINT("init htsize: %i (%i); memsize: %lu (%lu); \n", clampi.htsize, CL_HT_ENTRIES(cache), clampi.memsize, CL_MEM_SIZE(cache));


    return CL_SUCCESS;

}

void reset_data_structures(cl_cache_t * cache){

    SRAND(SEED);

    /* empty the free region index */
    Tree_Empty(cache->free_idx);

    /* setting the pending ptr to the pending queue tail */
    PEN_PTR(*win, cache) = PEN_BOTTOM(*win); 

    /* total number of observed gets */
    cache->getcount=0;

    /* moving avg */
    cache->mms=0;

    /* number of entries in HT */
    cache->table_size=0;    


    /* check memory allocation */    
    if (MEMORY(cache) == NULL) { printf("Error allocating memory\n"); exit(-1); }
     
    /* init HT */ 
    for (int i=0; i<CL_HT_ENTRIES(cache); i++) {
        cache->table[i].lasthit = 0;
    }

    for (int i=1; i < CL_HT_ENTRIES(cache)*2; i++) {
        TABLEENTRY(cache, i)->index = i;
        TABLEENTRY(cache, i)->mprev = NOENTRY;
        TABLEENTRY(cache, i)->mnext = NOENTRY;
        cache->free_entries[i-1] = i;   
    }

    cache->next_free_entry = CL_HT_ENTRIES(cache)*2 - 2;    

    /* set the first entry as a big memory region */
    TABLEENTRY(cache, 0)->type = FREE;
    TABLEENTRY(cache, 0)->size = cache->mem_size;
    TABLEENTRY(cache, 0)->offset = 0;
    TABLEENTRY(cache, 0)->index = 0;
    TABLEENTRY(cache, 0)->mprev = NOENTRY;
    TABLEENTRY(cache, 0)->mnext = NOENTRY;

    /* insert the empty region into the AVL */
    TABLEENTRY(cache, 0)->tnode = Tree_Insert(cache->free_idx, TABLEENTRY(cache, 0));
    
#ifdef CL_STATS
    //printf("cl_stats: %u\n", sizeof(cl_stats));
    memset(&(cache->stat), 0, sizeof(cl_stats));
#endif

}

