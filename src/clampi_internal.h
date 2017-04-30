#ifndef __CLAMPI_INTERNAL_H__
#define __CLAMPI_INTERNAL_H__

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "clampi.h"
#include "AVL/AvlTree.h"
//#include "utils.h"
#include "../config.h"

#ifdef CL_BENCH
    #include <liblsb.h>
#endif

#ifdef CLDEBUG
    #include <unistd.h>
#endif

/* Adaptive scheme parameters */
#ifdef CL_ADAPTIVE
    #define CL_HT_INC_THRESHOLD 0.1
    #define CL_HT_INC 1.5
    #define CL_HT_DEC_THRESHOLD 0.1
    #define CL_HT_DEC 0.75

    #define CL_SP_INC_THRESHOLD 0.1
    #define CL_SP_INC 2
#endif

/* Default HT size */
#ifndef CL_HT_ENTRIES_VAL
    #define CL_HT_ENTRIES_VAL 2000
#endif

/* Default memory buffer size */
#ifndef CL_MEM_SIZE_VAL
    #define CL_MEM_SIZE_VAL 128*1024*1024
#endif


/* Max items to scan during a capacity miss */
#ifndef CL_MAX_SCAN
    #define CL_MAX_SCAN 10
#endif


/* Every window should be associated with a pending queue.
   For now we assume that just one window is used by the application. */
#define CL_PENDING_STACK 64


#define CACHE_LINE 64                                                                                

#ifdef CLDEBUG
    #define CLPRINT(format, ...) printf("[RANK %i - %i] " format, dbg_rank, dbg_pid,  ##__VA_ARGS__)
    
    #define CENTRY_CHECK(c, ptr) \
        if (ptr<0 || ptr>CL_HT_ENTRIES(c)*2){ \
            printf("Error! ptr is not valid!\n"); \
            exit(-1); \
        }

    int dbg_rank;
    int dbg_pid;
#else
    #define CENTRY_CHECK(c, ptr)
    #define CLPRINT(format, ...) 
#endif

#define MPICHECK(X) {if (X!=MPI_SUCCESS) {printf("Error on "#X"\n"); MPI_Finalize(); exit(-1); }}

/* Cache/Memory descriptors status */
#define FREE 0
#define BUSY 1

/* Cache/Memory descriptors utility macros */
#define NEW_CENTRY(cache, entry) { \
    assert(cache->next_free_entry>=0); \
    uint32_t ptr = cache->free_entries[cache->next_free_entry--]; \
    CENTRY_CHECK(cache, ptr) \
    entry = TABLEENTRY(cache, ptr); \
}

#define FREE_CENTRY(cache, entry) cache->free_entries[++cache->next_free_entry] = entry->index;


/* Benchmarking macros */
#ifdef CL_BENCH
    #define TIMER_START LSB_Res();
    #define TIMER_STOP(id) LSB_Check(id);
#else
    #define TIMER_START
    #define TIMER_STOP(id)
#endif


/* Moving AVG ALPHA */
#define ALPHA 0.4


#define VOID char
#define ABS(a) ((a<0) ? -(a) : (a))

/* just for reproducitibilty */
#define SEED 1458334180

/* memory alloc/copy */
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#define CLALIGN(x) (DIV_ROUND_UP(x, CACHE_LINE)) * (CACHE_LINE)

#define USE_POSIX_MEMALIGN
#ifdef USE_POSIX_MEMALIGN
    //#define ALLOC(size, type, dest) { posix_memalign((void**) &dest, CACHE_LINE, size); }
    #define ALLOC(size, type, dest) { dest = (type *) aligned_alloc(CACHE_LINE, size); }

#else
    #define ALLOC(size, type, dest) { dest = (type *) malloc(size); }
#endif


#define CL_SSEMEMCPY
#ifdef CL_SSEMEMCPY
    #define MEMCPY(to, from, len) sse_memcpy(to, from, len)
#else
    #define MEMCPY(to, from, len) memcpy(to, from, len)
#endif

/* PRNG */
#ifdef CL_FASTRAND
    #define SRAND(SEED) fast_srand(SEED);
            #define RAND() fast_rand()
#else
    #define SRAND(SEED) srand(SEED);
    #define RAND() rand()
#endif

/* Score */
#ifdef SCORE_NOSPACE
    #define SCORE(L,P) ((double) L/cache->getcount)
#elif defined(SCORE_NOTIME)
    #define SCORE(L,P) (ABS(cache->mms - P)/(cache->mms))
#else
    #define SCORE(L,P) (ABS(cache->mms - P)/(cache->mms)) * ((double) L/cache->getcount)
#endif

/* utility macro for printing cache entry interval */
#define CLEFT(c) (c->offset)
#define CRIGHT(c) (c->offset + ((c->type==BUSY) ? c->get_info.size : c->size) - 1)


struct _cl_entry;

typedef struct _cl_get{
    int rank;
    int addr;
    uint32_t size;
} cl_get_t; 


typedef struct _cl_index{
    uint32_t lasthit;
    uint32_t index;
    uint16_t penalty;
} cl_index_t;



/* centry are used for cache entries and free regions */
/* TODO: the name is bad */
typedef struct _cl_entry{
    union{
        cl_get_t get_info; //8
        uint32_t size;
    }; 
    /* Why do I need this intermediate struct? 
    Because of the cuckoo hashing: ht entries can be moved and
    you would need to update mprev and mnext for each movement 
    done during the insertion phase. */

    uint8_t type; 
    int32_t htindex;
    uint32_t index; //centry index
    Node tnode; //used only if it's a free region descriptor

    int32_t mprev;
    int32_t mnext;

    uint32_t offset;
    //int size; //4

    /* This will be accessed during the flush */
    int8_t pending; /* -1 if copy not completed. A rank otw. */

} cl_entry_t; //28



typedef struct _cl_pending{
    void * target_addr;
    /* This is set to NULL if the entry was evicted while the copy was still pending. */
    void * origin_addr;

    int entry; 
    int size;

} cl_pending_t;

typedef struct _cl_cache{
    void * mem; 
    double mms;
    uint32_t getcount;
    cl_entry_t * table_ptr;

    uint32_t * free_entries;
    uint32_t next_free_entry;    

    Tree free_idx;
    
    uint32_t ht_entries;

    uint32_t mem_size;
    cl_index_t * table;

    uint32_t table_size;

    int32_t penptr;
    cl_pending_t pending[CL_PENDING_STACK];

#ifdef CL_STATS
    cl_stats stat;
#endif

#ifdef CL_ADAPTIVE
    FILE * stat_file;
#endif

} cl_cache_t;

#ifdef CL_ADAPTIVE
int cl_adapt(CMPI_Win win, int reset);
#endif

void reset_data_structures(cl_cache_t * cache);

int free_entry_comp(void * a, void * b);
void free_entry_print(void * context, void * a);
void free_entry_replace(void * target, void * source);


/* CLaMPI configuration */
typedef struct cl_conf{
    uint8_t init;
    uint32_t htsize; /* Starting HT size */
    uint32_t memsize; /* Memory buffer size (in bytes) */
} cl_conf_t;
cl_conf_t clampi;

/* Utility macros */
#define CL_HT_ENTRIES(c) (c->ht_entries)
#define CL_MEM_SIZE(c) (c->mem_size)
#define TABLEPTR(c) (c->table_ptr)
#define MEMORY(c) (c->mem)
#define TABLEENTRY(c, i) (TABLEPTR(c) + i)
#define HT(c, e) (c->table[e->htindex])
#define NOENTRY -1


/* Pending entries (i.e., in flight) */
#define PEN_BOTTOM(win) 0
#define PEN_TOP(win) CL_PENDING_STACK
#define PEN_PTR(win, cache) (cache->penptr)


#endif /* __CLAMPI_INTERNAL_H__ */
