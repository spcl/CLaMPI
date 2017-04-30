#ifndef __CLAMPI_H__
#define __CLAMPI_H__

#include <stdio.h>
#include <stdint.h>
#include "mpi_wrapper.h"

#define CL_SUCCESS 0
#define CL_HIT 1
#define CL_CACHED 2
#define CL_CACHED_R_HT 3
#define CL_CACHED_R_SPACE 4
#define CL_NOT_CACHED 5
#define CL_NO_BIN_FOUND 6
#define CL_TOO_BIG 7
#define CL_HASHTABLE_FULL 8
#define CL_IN_FLIGHT 9
#define CL_NOT_INDEXED 10
#define CL_FAIL 11
#define CL_NOT_LOCKED 12
#define CL_LOCAL 13
#define CL_ERROR 14
#define CL_FLUSHED 15

//#ifdef CL_STATS
/* get failed due to space, caused an eviction due to capacity */
#define CL_NOT_CACHED_SPACE 16
/* get failed due to space, caused an eviction due to conflict */
#define CL_NOT_CACHED_HT 17
//#endif

#define CL_BITMAP_SEARCH 49
#define CL_BITMAP_SEARCH2 50
#define CL_VICTIM_SELECTION 51
#define CL_EVICTION 52
#define CL_DATA_COPY 53
#define CL_CACHING 54
#define CL_MPI_FLUSH 55
#define CL_MPI_GET 56
#define CL_LOOKUP 57
#define CL_AVL_SEARCH 58

typedef struct _cl_win{
    MMPI_WIN win;
    void * cache;

#ifdef NODESHARING
    MPI_Win wtable;
    int crank;
    int csize;
#endif

} CMPI_Win;


typedef struct _cl_stats{
    uint32_t hits; 
    uint32_t failed; /* failed due to space */
    uint32_t ht_evictions; /* evictions due HT conflicts */
    uint32_t sp_evictions; /* eviction due to space */
    uint32_t evicted_size; /* sum of the sizes of evicted entries */
    uint32_t cached; /* insertions */ 
    uint32_t locals; /* locals (only meaningful when NODESHARING). Not included in getcount. */
    uint32_t last_eviction_scan; /* number of visited ht entries for the last eviction */
    uint32_t last_eviction_sample; /* number of non-empty visited ht entries for the last eviction */
    uint32_t tot_eviction_scan; /* number of visited ht entries for the last eviction */
    uint32_t tot_eviction_sample; /* number of non-empty visited ht entries for the last eviction */


    uint32_t last_ht_scan; /* number of conflicting entries in the last insert (updated only when a new conflicting access happens). */
    uint64_t total_data; //data (in bytes) transfered from the network.
} cl_stats;


typedef struct _cl_stats_full{
    cl_stats base;
    int entries;
    int free_regions;
    unsigned int total;
    int htsize;
    int mem_size;
    double load_factor;
    double mms;
    unsigned int directs;
} cl_stats_full;

void cl_init();

int CMPI_Get(void * origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, CMPI_Win win);

int CMPI_Win_create(void * base, MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm, CMPI_Win * win);
int CMPI_Win_allocate(MPI_Aint size, int disp_unit, MPI_Info info, MPI_Comm comm, void * baseptr, CMPI_Win * win);

int CMPI_Win_free(CMPI_Win * win);
int CMPI_Win_invalidate(CMPI_Win win);


//int cl_fence(int predicate, MMPI_WIN win);
int CMPI_Win_flush(int rank, CMPI_Win win);

int cl_flush(CMPI_Win win);
int cl_reset(CMPI_Win win); //reset adaptive stuff
int cl_get_stats(CMPI_Win win, cl_stats_full * stats);
void cl_print_stats(FILE * fp, CMPI_Win win);
void cl_print_val(FILE * fp, const char * name, int val, CMPI_Win win);
void cl_get_buff_stats(CMPI_Win win, int * blocks, int * space, double * avg, double * ratio, int * entries);

void cl_set_stat_file(FILE * fp, CMPI_Win win);


//int cl_finalize();

#endif /* __CLAMPI_H__ */
