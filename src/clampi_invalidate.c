#include "clampi_internal.h"

int CMPI_Win_invalidate(CMPI_Win win){
    cl_cache_t * cache = (cl_cache_t *) win.cache;

    CLPRINT("\n\n@@@FLUSHING@@@\n\n");    
    reset_data_structures(cache);

    return CL_SUCCESS;   
}


