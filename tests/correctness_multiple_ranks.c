#include <mpi_wrapper.h>
#include <clampi.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


#define MIN(a, b) (a<b ? a : b)

int main(int argc, char * argv[]){

    if (argc!=3){
        printf("Usage: %s <num_ints> <iterations>\n", argv[0]);
        exit(-1);
    }

    MMPI_INIT(&argc, &argv);

    int num_ints = atoi(argv[1]);
    int iterations = atoi(argv[2]);
    size_t win_size = (size_t) (num_ints) * sizeof(int);

  
    /* prepare buffer */    
    int * buffer;
    int * dest_buffer = (int *) malloc(win_size);
    int * dest_buffer_check = (int *) malloc(win_size);
    int rank, peers;

    MPI_Comm_size(MPI_COMM_WORLD, &peers);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    srand(time(NULL)*(rank+1));

    /* create MPI and CMPI win */
    CMPI_Win cmpi_win;

    CMPI_Win_allocate(win_size, sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &buffer, &cmpi_win); 
        
    for (int i=0; i<num_ints; i++) {
        buffer[i] = rand() % 100;
    }


    MPI_Barrier(MPI_COMM_WORLD);

    /* Start test */
    MMPI_WIN_LOCK_ALL(0, cmpi_win.win);

    int error=0;
       


    for (int i=0; i<iterations; i++){
        int offset = rand() % num_ints;
        int size = MIN(rand(), num_ints - offset);
        int peer = (rand() % (peers-1)) + 1;   
        
        //printf("[%i] get from %i; offset: %i; size: %i; i: %i\n", rank, peer, offset, size, i);
        /* get data */
        CMPI_Get(dest_buffer, size, MPI_INT, peer, offset, size, MPI_INT, cmpi_win);
        MMPI_GET(dest_buffer_check, size, MPI_INT, peer, offset, size, MPI_INT, cmpi_win.win);
        
        /* flush window */
        CMPI_Win_flush(peer, cmpi_win);

        /* check */
        for (int j=0; j<size; j++){
            if (dest_buffer[j] != dest_buffer_check[j]){
                error=1;
                printf("Error: expected: %i; got: %i;\n", dest_buffer[j], dest_buffer_check[j]);
            }
        }
        if (i%500==0){ printf("Rank: %i; i: %i;\n", rank, i);}

    }
    
    
    MMPI_WIN_UNLOCK_ALL(cmpi_win.win);
    MPI_Barrier(MPI_COMM_WORLD);

    cl_print_stats(stdout, cmpi_win);

    /* Finalize */
    free(dest_buffer);
    free(dest_buffer_check);
    CMPI_Win_free(&cmpi_win);
    MMPI_FINALIZE();

    if (!error) printf("PASSED!\n");

    return 0;
}

