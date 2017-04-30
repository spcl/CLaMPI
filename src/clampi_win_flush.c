
#include "clampi_internal.h"
#include "utils.h"

int CMPI_Win_flush(int rank, CMPI_Win win){

    cl_cache_t * cache = (cl_cache_t *) win.cache;

    TIMER_START
    int res = MMPI_WIN_FLUSH(rank, win.win);
    TIMER_STOP(CL_MPI_FLUSH)
    int curr;
    
    TIMER_START
    //empty the pending queue

    curr = PEN_PTR(win, cache) - 1;

    int bottom = PEN_BOTTOM(win);

    while (curr>=bottom){
        CLPRINT("flushing: curr: %i; bottom: %i\n", curr, bottom);

        if (TABLEENTRY(cache, cache->pending[curr].entry)->pending==curr){
            TABLEENTRY(cache, cache->pending[curr].entry)->pending=-1;
            
            MEMCPY(
                (VOID *) cache->pending[curr].target_addr, 
                (VOID *) cache->pending[curr].origin_addr, 
                cache->pending[curr].size
            );
        }

        curr--;
    }

    PEN_PTR(win, cache) = curr+1;

    TIMER_STOP(CL_DATA_COPY)
    return res;   
}

