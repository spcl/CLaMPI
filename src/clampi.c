#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <time.h>
#include <limits.h>

#include "clampi.h"
#include "clampi_internal.h"
#include "mpi_wrapper.h"


/**** Initialization functions ****/
int _cl_win_init(CMPI_Win *  win, MPI_Comm comm);

void cl_init(){

    clampi.init=1;

    SRAND(SEED);

    clampi.htsize = CL_HT_ENTRIES_VAL;
    char * htsize_str = getenv("CL_HT_ENTRIES");
    if (htsize_str!=NULL) clampi.htsize = atoi(htsize_str);

    clampi.memsize = CL_MEM_SIZE_VAL;
    char * memsize_str = getenv("CL_MEM_SIZE");
    if (memsize_str!=NULL) {
        char * memsize_str_end;
        int errno = 0;

        clampi.memsize = (uint64_t) strtoull(memsize_str, &memsize_str_end, 10);

         /* str was not a number */
        assert(clampi.memsize != 0 || memsize_str != memsize_str_end);

        /* the value of str does not fit in unsigned long long */
        assert(clampi.memsize < ULLONG_MAX || !errno);

        /* str began with a number but has junk left over at the end */
        assert(!*memsize_str_end);
    }

#ifdef CLDEBUG
    MPI_Comm_rank(MPI_COMM_WORLD, &dbg_rank);
    dbg_pid = getpid();
#endif

#ifdef CL_ADAPTIVE
    printf("CLAMPI: Adaptive scheme enabled!\n");
#endif

    printf("CLAMPI INIT: mem_size: %lu; cache line: %i;  HT entries: %i\n", clampi.memsize, CACHE_LINE, clampi.htsize);

#ifdef USE_FOMPI
    printf("CLAMPI: Using foMPI\n");
#endif 

}


