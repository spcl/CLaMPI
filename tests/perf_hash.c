#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <string.h>

#include <liblsb.h>
#include "clampi.h"
#include <time.h>
#include "mpi_wrapper.h"

#include <complex.h>
#include "utils/normal.h"
#include "../src/clampi_internal.h"

#ifndef TOT
#define TOT 100
#endif

#ifndef NPOS
#define NPOS 1000
#endif

#ifndef NGETS
#define NGETS 100000
#endif

#define WINDISPL sizeof(int32_t)
#define WINCOUNT 1024*1024
#define WINSIZE WINCOUNT*WINDISPL

#define MPICHECK(X) {if (X!=MPI_SUCCESS) {printf("Error on "#X"\n"); MPI_Finalize(); exit(-1); }}

#define BMPI 0
#define BCLAMPI 1
#define BDISCARD 2

//#define CHECK_CORRECTNESS

#define MAXRES 20

#define FBSIZE 1024*1024*10

void * win_mem;
void * flushbuff;
char * scoretype;


volatile int acc;

char * results[] = {"SUCCESS", "HIT", "CACHED", "CACHED_HT", "CACHED_SPACE", "NOT_CACHED", "NO_BIN_FOUND", "TOO_BIG", "HASHTABLE_FULL", "IN_FLIGHT", "NOT_INDEXED", "FAIL", "NOT_LOCKED", "LOCAL", "ERROR", "NOT_CACHED_SPACE", "NOT_CACHED_HT"};


void flush_system_cache(){

    char * buff = (char *) flushbuff;
    //acc = 0;
    int res=0;
    for (int i=0; i<FBSIZE; i++){
        res += (int) buff[i];
        //__asm__ __volatile__("");
    }
    acc=res;
}




void bench_overlap(int type, int stage, double wait_time, void * loc_mem, int * start, int * sizes, int * gets, int num, CMPI_Win win){

    int rank, res=0;    
    //double wait_time;


    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    //printf("overlap\n");
    LSB_Set_Rparam_string("type", (type==BMPI ? "MPI" : "CLAMPI"));
    LSB_Set_Rparam_int("stage", stage);


    //double wait_time[MAXRES]; 
    
    MMPI_WIN_LOCK(MPI_LOCK_SHARED, 1, 0, win.win);

    if (stage==1) {
        /* latency */
        for (int i=0; i<num; i++){
            int cstart = start[gets[i]];
            int csize = sizes[gets[i]];

            if (rank==0){        
                res=0;
            
                LSB_Set_Rparam_int("size", csize*4);
            
                if (type==BCLAMPI){
                    LSB_Res();
                    res = CMPI_Get(loc_mem, csize, MPI_INT, 1, cstart, csize, MPI_INT, win);
                    CMPI_Win_flush(1, win);
                    LSB_Rec(res);
                }else if (type==BMPI){
                    LSB_Res();
                    MMPI_GET(loc_mem, csize, MPI_INT, 1, cstart, csize, MPI_INT, win.win);
                    MMPI_WIN_FLUSH(1, win.win);
                    LSB_Rec(0);
                }else{
                    MMPI_GET(loc_mem, csize, MPI_INT, 1, cstart, csize, MPI_INT, win.win);
                    MMPI_WIN_FLUSH(1, win.win);
                }

                //LSB_Set_Rparam_int("get_result", res);
                //LSB_Next();
            }
        }
    } else {

        
        for (int i=0; i<num; i++){
            int cstart = start[gets[i]];
            int csize = sizes[gets[i]];
            double wtime;


            if (rank==0){        
                res=0;
            
                LSB_Set_Rparam_int("size", csize*4);
 

                if (type==BCLAMPI){           
                    LSB_Res();
                    res = CMPI_Get(loc_mem, csize, MPI_INT, 1, cstart, csize, MPI_INT, win);
		    LSB_Rec(res);

                    wtime = LSB_Wait(wait_time);

		    LSB_Res();
                    CMPI_Win_flush(1, win);
                    LSB_Rec(res);
                }else if(type==BMPI){
                    LSB_Res();
                    MMPI_GET(loc_mem, csize, MPI_INT, 1, cstart, csize, MPI_INT, win.win);
		    LSB_Rec(0);

                    wtime = LSB_Wait(wait_time);

		    LSB_Res();
                    MMPI_WIN_FLUSH(1, win.win);
                    LSB_Rec(0);
                }else{
                    MMPI_GET(loc_mem, csize, MPI_INT, 1, cstart, csize, MPI_INT, win.win);
                    MMPI_WIN_FLUSH(1, win.win);
                }

                //LSB_Set_Rparam_double("wait_time_ask", wait_time);
                //LSB_Set_Rparam_double("wait_time_real", wtime);

                //LSB_Next();
            }
        }
    }

    MMPI_WIN_UNLOCK(1, win.win);

}


void bench(int type, void * loc_mem, int * start, int * sizes, int * gets, int num, CMPI_Win win){

    int res;
    int rank; 

    int status = 0;
    int totfail=0;
    

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    LSB_Set_Rparam_string("type", (type==BMPI ? "MPI" : "CLAMPI"));

    LSB_Set_Rparam_string("state", (type==BMPI ? "NA" : "TRANSIENT"));

    for (int i=0; i<num; i++){
        int cstart = start[gets[i]];
        int csize = sizes[gets[i]];

        if (rank==0){        
            res=0;

#ifdef PER_GET            
            LSB_Set_Rparam_int("size", csize*4);
            LSB_Set_Rparam_int("iter1", i); //get sequence number
            LSB_Set_Rparam_int("get_idx", gets[i]); //index of the current get. Identifes a specific get.
#endif

            //printf("i: %i; gets[i]: %i;\n", i, gets[i]);
            MMPI_WIN_LOCK(MPI_LOCK_SHARED, 1, 0, win.win);
            //printf("main lock ok\n");
            //printf("i: %i; cstart: %i; csize: %i\n", i, cstart, csize);   
            //printf("flushing\n");
#ifndef NOFLUSH
            flush_system_cache();
#endif
            //printf("flushing ok\n");

            //LSB_Res();

            if (type==BCLAMPI){


#ifdef PER_GET
                LSB_Set_Rparam_string("type", "GET");
#ifndef CL_BENCH
                LSB_Res();
#endif
#endif /* PER_GET */

                res = CMPI_Get(loc_mem, csize, MPI_INT, 1, cstart, csize, MPI_INT, win);

#ifdef PER_GET
#ifndef CL_BENCH
                LSB_Rec(res);
#else
		LSB_Set_Rparam_int("get_result", res);
	        LSB_Next();
#endif

                LSB_Set_Rparam_string("type", "FLUSH");

#ifndef CL_BENCH
                LSB_Res();
#endif
#endif /* PER_GET */
                CMPI_Win_flush(1, win);
#ifdef PER_GET
#ifndef CL_BENCH
                LSB_Rec(res);
#else
		LSB_Next();
#endif
                if (!status && (res==CL_NOT_CACHED || res==CL_CACHED_R_HT || res==CL_CACHED_R_SPACE)) {
                    LSB_Set_Rparam_string("state", "STEADY");
                    status=1;
                }

#ifdef CL_BUFF_STATS
                cl_stats_full stats;
                cl_get_stats(win, &stats);
                int blocks, space, entries;
                double ratio, avg;
                totfail += res==CL_NOT_CACHED;
                cl_get_buff_stats(win, &blocks, &space, &avg, &ratio, &entries);
                int last_scan = (res==CL_CACHED_R_SPACE || res==CL_NOT_CACHED_SPACE) ? stats.base.last_eviction_scan : 0;
                int last_sample = (res==CL_CACHED_R_SPACE || res==CL_NOT_CACHED_SPACE) ? stats.base.last_eviction_sample : 0;
                int last_ht_scan = (res==CL_CACHED_R_HT || res==CL_NOT_CACHED_HT) ? stats.base.last_ht_scan : 0;
                char * resstr = results[res];


                printf("[BUFF] %i %i %i %lf %lf %i %i %i %i %i\n", i, blocks, space, avg, ratio, entries, csize*4, totfail, last_scan, last_sample);
                printf("[GET][Rformat] %i %i %i %i %s %s %s %i\n", i, CL_MAX_SCAN, stats.htsize, CL_MEM_SIZE_VAL, scoretype, resstr, "SPACE", space);
                printf("[GET][Rformat] %i %i %i %i %s %s %s %lf\n", i, CL_MAX_SCAN, stats.htsize, CL_MEM_SIZE_VAL, scoretype, resstr, "AVG_FREE_REGION", avg);
                printf("[GET][Rformat] %i %i %i %i %s %s %s %lf\n", i, CL_MAX_SCAN, stats.htsize, CL_MEM_SIZE_VAL, scoretype, resstr, "SPACE_RATIO", ratio);
                printf("[GET][Rformat] %i %i %i %i %s %s %s %i\n", i, CL_MAX_SCAN, stats.htsize, CL_MEM_SIZE_VAL, scoretype, resstr, "HT_ENTRIES", entries);
                printf("[GET][Rformat] %i %i %i %i %s %s %s %i\n", i, CL_MAX_SCAN, stats.htsize, CL_MEM_SIZE_VAL, scoretype, resstr, "GET_SIZE", csize*4);
                printf("[GET][Rformat] %i %i %i %i %s %s %s %i\n", i, CL_MAX_SCAN, stats.htsize, CL_MEM_SIZE_VAL, scoretype, resstr, "FAILED_GETS", totfail);
                printf("[GET][Rformat] %i %i %i %i %s %s %s %i\n", i, CL_MAX_SCAN, stats.htsize, CL_MEM_SIZE_VAL, scoretype, resstr, "LAST_SCAN", last_scan);
                printf("[GET][Rformat] %i %i %i %i %s %s %s %i\n", i, CL_MAX_SCAN, stats.htsize, CL_MEM_SIZE_VAL, scoretype, resstr, "LAST_SAMPLE", last_sample);
                printf("[GET][Rformat] %i %i %i %i %s %s %s %i\n", i, CL_MAX_SCAN, stats.htsize, CL_MEM_SIZE_VAL, scoretype, resstr, "LAST_HT_SCAN", last_ht_scan);
                printf("[GET][Rformat] %i %i %i %i %s %s %s %i\n", i, CL_MAX_SCAN, stats.htsize, CL_MEM_SIZE_VAL, scoretype, resstr, "GET_INDEX", gets[i]);
                //printf("[GET][Rformat] %i %i %i %i %s %s %s\n", i, CL_MAX_SCAN, CL_HT_ENTRIES_VAL, CL_MEM_SIZE, scoretype, "GET_RESULT", results[res]);

#endif               
#endif /* PER_GET */

            }else if (type==BMPI){
#ifdef PER_GET
                LSB_Set_Rparam_string("type", "GET");
		LSB_Set_Rparam_int("get_result", res); //to indicate MPI get
                LSB_Res();
#endif
                MMPI_GET(loc_mem, csize, MPI_INT, 1, cstart, csize, MPI_INT, win.win);
#ifdef PER_GET
                LSB_Rec(CL_MPI_GET);                
                LSB_Set_Rparam_string("type", "FLUSH");
                LSB_Res();
#endif
                MMPI_WIN_FLUSH(1, win.win);
#ifdef PER_GET
                LSB_Rec(CL_MPI_FLUSH);
#endif
            }else{ /* type==BDISCARD */
                MMPI_GET(loc_mem, csize, MPI_INT, 1, cstart, csize, MPI_INT, win.win);
                MMPI_WIN_FLUSH(1, win.win);
            }


            MMPI_WIN_UNLOCK(1, win.win);

        }/*else if (type==BCLAMPI) {
            cl_fence(0, win);
        }else MMPI_WIN_FENCE(0, win);
        */
#ifdef CHECK_CORRECTNESS
        if (rank==0){
            int error=0;
            for (int j=0; j<csize; j++){
                if (((int *) loc_mem)[j] != ((int *) win_mem)[cstart + j] || error){
                    printf("Error! Get n.: %i; Byte: %i; Expected: %i; Received: %i\n", i, j, ((int *) win_mem)[j], ((int *) loc_mem)[j]);
                    error=1;
                }
            }
            if (error) exit(-1);
        }
#endif
    }

#ifdef BDUMP

    cl_bitmap_dump(win, "dump");
#endif

}



void bench_time_main(void * loc_mem, int * start, int * sizes, int * gets, int num, CMPI_Win win){

    int rank;


    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank==0) printf("Warmup\n");
    //Warmup
    bench(BDISCARD, loc_mem, start, sizes, gets, num, win);
    
    //MPI_Barrier(MPI_COMM_WORLD);

#ifndef NOMPI
    if (rank==0) printf("MPI\n");
    for (int i=0; i<TOT; i++){
        if (rank==0) { printf("MPI it %i of %i\n", i, TOT); fflush(stdout); }
#ifndef PER_GET
	    LSB_Res();
#else
        LSB_Set_Rparam_int("iter", i);
#endif
        bench(BMPI, loc_mem, start, sizes, gets, num, win);
#ifndef PER_GET
	    LSB_Rec(i);
#endif
    }
    //MPI_Barrier(MPI_COMM_WORLD);
#endif


    if (rank==0) printf("CLAMPI\n");
    for (int i=0; i<TOT; i++){
        if (rank==0) { printf("CLAMPI it %i of %i\n", i, TOT); fflush(stdout); }
#ifndef PER_GET
	    LSB_Res();
#else
        LSB_Set_Rparam_int("iter", i);
#endif
        bench(BCLAMPI, loc_mem, start, sizes, gets, num, win);
#ifndef PER_GET
    	LSB_Rec(i);
#endif

        if (rank==0) {
#ifdef CL_STATS
            cl_print_stats(stdout, win);
#endif
            //LSB_Set_Rparam_string("type", "CACHE_FLUSH");
            //LSB_Res();
	        cl_reset(win);
            CMPI_Win_invalidate(win);
            //cl_flush(win);
            //LSB_Rec(0);
        }
    }

}


void bench_overlap_main(void * loc_mem, int * start, int * sizes, int * gets, int num, CMPI_Win win){

    int rank;
    double wait_time, tmp;


    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank==0) printf("Warmup\n");
    bench_overlap(BDISCARD, 1, 0, loc_mem, start, sizes, gets, num, win);   

    CMPI_Win_invalidate(win);
    
    if (rank==0) printf("MPI - stage 1\n");
    for (int i=0; i<TOT; i++){
        //if (rank==0) { printf("MPI it %i of %i\n", i, TOT); fflush(stdout); }
        LSB_Set_Rparam_int("iter", i);
        bench_overlap(BMPI, 1, 0, loc_mem, start, sizes, gets, num, win);   
    }

    //MPI_Barrier(MPI_COMM_WORLD);


    if (rank==0) printf("CLAMPI - stage 1\n");
    for (int i=0; i<TOT; i++){
        //if (rank==0) { printf("CLAMPI it %i of %i\n", i, TOT); fflush(stdout); }
        LSB_Set_Rparam_int("iter", i);
        bench_overlap(BCLAMPI, 1, 0, loc_mem, start, sizes, gets, num, win);
        if (rank==0) CMPI_Win_invalidate(win);
    }


    
    for (int i=0; i<MAXRES; i++){
        LSB_Fold(i, LSB_MAX, &tmp);
        //wait_time[i] *= 2;
	wait_time = (i==0 || wait_time<tmp) ? tmp : wait_time;
    }
    wait_time*=2;
    printf("waiting time: %lf\n", wait_time);
    fflush(stdout);

    
    //LSB_Flush();

    if (rank==0) printf("MPI - stage 2\n");
    fflush(stdout);

    for (int i=0; i<TOT; i++){
        //if (rank==0) { printf("MPI it %i of %i\n", i, TOT); fflush(stdout); }
        LSB_Set_Rparam_int("iter", i);
        bench_overlap(BMPI, 2, wait_time, loc_mem, start, sizes, gets, num, win);
    }

    //MPI_Barrier(MPI_COMM_WORLD);


    if (rank==0) printf("CLAMPI - stage 2\n");
    fflush(stdout);

    for (int i=0; i<TOT; i++){
        //if (rank==0) { printf("CLAMPI it %i of %i\n", i, TOT); fflush(stdout); }
        LSB_Set_Rparam_int("iter", i);
        bench_overlap(BCLAMPI, 2, wait_time, loc_mem, start, sizes, gets, num, win);
        if (rank==0) CMPI_Win_invalidate(win);
    }
}



int main(int argc, char * argv[]){

    CMPI_Win win;
    int rank;
    int msg_size = 8; //num of ints
    MMPI_INIT(&argc, &argv);
    LSB_Init("test_clampi", 0);

    int seed = 12445678;  
    srand(seed);
    //srand(time(NULL));
    cl_init();

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    int overlap = argc==2;

    if (!overlap) printf("Test: costs breakdown (%i)\n", argc);
    else printf("Test: overlap\n");

    if (rank==0){
#ifdef USE_FOMPI 
        printf("CLAMPI: Using foMPI.\n");
#else 
        printf("CLAMPI: Not using foMPI\n");
#endif
    }
    void * loc_mem;
    posix_memalign(&loc_mem, 128, WINSIZE);
    flushbuff = malloc(FBSIZE);

    //void * win_mem;// = malloc(WINSIZE);
    //memset(win_mem, rank, WINSIZE);


#ifdef SCORE_NOSPACE
    scoretype = "NOSPACE";
#elif defined(SCORE_NOTIME)
    scoretype = "NOTIME";
#else
    scoretype = "FULL";
#endif


    int v,s;
    MPI_Get_version(&v, &s);
    printf("MPI version: %i; subversion: %i\n", v, s);

    int n = 1;
    // little endian if true
    if(*(char *)&n == 1) { printf("LITTLE ENDIAN\n");}
    else printf("BIG ENDIAN\n");
    MPI_Info info;
    MPI_Info_create(&info);
    MPI_Info_set(info, CLAMPI_MODE, CLAMPI_USER_DEFINED);

    //MPICHECK(MPI_Win_create(win_mem, WINSIZE, WINDISPL, MPI_INFO_NULL, MPI_COMM_WORLD, &win));
    MPICHECK(CMPI_Win_allocate(WINSIZE, WINDISPL, info, MPI_COMM_WORLD, &win_mem, &win));


    //MMPI_WIN_FENCE(0, win);   


    //int len=16;
    //int start[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 3, 13, 14};
    ///int sizes[] = {4, 4, 8, 8, 8, 32, 32, 32, 32, 4, 8, 8, 4, 8, 4, 32};


    int start[NPOS];
    int sizes[NPOS];
    int gets[NGETS];

    int npsizes = 15;
    int psizes[] = {4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768, 65536};


    for (int i=0; i<NPOS; i++){
        start[i] = i;
        sizes[i] = psizes[rand() % npsizes] / 4 /* sizeof(int32_t) */;
    }


    int stop=0, count=0, i;
    

#ifdef NORMAL_SAMPLING
    int mu = NPOS/2;
    int si = mu/4;
#endif

    /* Create NGETS with sizes randomily selected from psizes[] */
    for (int i=0; i<NGETS; i++){
#ifdef NORMAL_SAMPLING
	gets[i] = i4_normal_ab(mu, si, &seed) % NPOS;
#else
        gets[i] = rand() % NPOS;
#endif
    }


    /* Initialize the window with random values */
    for (int i=0; i<WINCOUNT; i++){
        ((int32_t *) win_mem)[i] = (int32_t) rand();
    }


    char * benchid = getenv("BENCH_ID");
    if (benchid!=NULL) LSB_Set_Rparam_string("bench_id", benchid);

    char * htsize_str = getenv("CL_HT_ENTRIES");
    int htsize = CL_HT_ENTRIES_VAL;
    if (htsize_str!=NULL) htsize=atoi(htsize_str); 

    LSB_Set_Rparam_int("htsize", htsize);
    LSB_Set_Rparam_int("sample_size", CL_MAX_SCAN);
    MPI_Barrier(MPI_COMM_WORLD);


    
    if (rank==0){
        if (overlap) bench_overlap_main(loc_mem, start, sizes, gets, NGETS, win);
        else bench_time_main(loc_mem, start, sizes, gets, NGETS, win);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    if (rank==0) printf("END\n");
    CMPI_Win_free(&win);
 
    //if (rank==0) printf("HITS: %i (MAX: %i); CACHED: %i; CACHED (replace): %i; FAILED: %i\n", resv[CL_HIT], NGETS-NPOS, resv[CL_CACHED], resv[CL_CACHED_R], resv[CL_NOT_CACHED]);
    //cl_finalize();
    LSB_Finalize();
    MMPI_FINALIZE();

}




