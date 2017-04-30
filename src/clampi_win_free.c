
#include "clampi_internal.h"

int CMPI_Win_free(CMPI_Win * win){
    cl_cache_t * cache = (cl_cache_t *) win->cache;

    MMPI_WIN_FREE(&win->win);
    win->cache = NULL;
    /*FIXME: free stuff here */
    return CL_SUCCESS;
}   

