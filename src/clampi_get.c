#include "clampi_internal.h"
#include "ht.h"
#include "utils.h"
#include <stdlib.h>


int CMPI_Get(void * origin_addr, int origin_count, MPI_Datatype origin_datatype, int target_rank, MPI_Aint target_disp, int target_count, MPI_Datatype target_datatype, CMPI_Win win){

    /* get the cache descriptor */
    cl_cache_t * cache = (cl_cache_t *) win.cache;

    /* compute the size. TODO: fix it */
    MPI_Aint mpi_extent, lb;
    MPI_Type_get_extent(origin_datatype, &lb, &mpi_extent);
    uint32_t size = mpi_extent * origin_count;


    CLPRINT("GET; cache: %p; rank: %i; addr: %lu; size: %u\n", cache, target_rank, target_disp, size);

    /* update cache core stats */
    cache->getcount += 1;
    cache->mms = ALPHA*cache->mms + (1-ALPHA)*size;

    /* lookup */
    TIMER_START
    int key = HT_KEY(target_rank, target_disp, size);
    cl_entry_t * el = ht_lookup(cache, target_rank, target_disp, size);
    TIMER_STOP(CL_LOOKUP)

#ifdef CLDEBUG
    if (el!=NULL && el->pending>=0) {
        CLPRINT("CACHE HIT BUT PENDING. MAKING NEW NOT-CACHED GET\n");
    }
#endif

    /* CACHE HIT! */
    if (el!=NULL && el->pending<0){ /*found it in el && not in flight*/

        CLPRINT("CACHE HIT: offset: %i; cached addr: %p\n", el->offset, MEMORY(cache) + el->offset);
        TIMER_START
        MEMCPY((VOID *) origin_addr, (VOID *) MEMORY(cache) + el->offset, el->get_info.size);
        //el->lasthit = cache->getcount;
 	    TIMER_STOP(CL_DATA_COPY)
#ifdef CL_STATS
        cache->stat.hits++;
#endif               
        CLPRINT("-----------------\n\n");
        return CL_HIT;   
    }

    /* CACHE MISS! */        
    CLPRINT("CACHE MISS! \n");

    int idx = ((RAND() % CL_HT_ENTRIES(cache)) / HT_ENTRIES_PER_CACHELINE) * HT_ENTRIES_PER_CACHELINE;
    //PREFETCH(&(cache->table[idx]), CL_MAX_SCAN/HT_ENTRIES_PER_CACHELINE);

    /* make the real get */
    TIMER_START
    MMPI_GET(origin_addr, origin_count, origin_datatype, target_rank, target_disp, target_count, target_datatype, win.win);
    TIMER_STOP(CL_MPI_GET);

#ifdef CL_STATS
    cache->stat.total_data += size;
#endif

    /* Do not cache if it's already in flight 
    TODO: no get should be issued in this case (i.e., partial hit) */
    if (el!=NULL) {
        CLPRINT("Data is in flight!\n-----------------\n\n");
        return CL_IN_FLIGHT;
    }
    /* Check if we can cache this message */
    if (size>CL_MEM_SIZE(cache)) {
        CLPRINT("Warning: message too big (message size: %lu; cache size: %lu)!\n", size, CL_MEM_SIZE(cache));
        CLPRINT("-----------------\n\n");
        return CL_NOT_CACHED;
    }


    /*==== Try to allocate space ====*/
    TIMER_START
    /* find free region */
    cl_entry_t tosearch;
    tosearch.size = size; 
#ifdef CLDEBUG
    //Tree_Print(cache->free_idx);  
#endif
    cl_entry_t * free_region = (cl_entry_t *) Tree_CSearchNode(cache->free_idx, &tosearch); 

    /* if free_region!=NULL, we are sure that we are going to use this free_region. Recall that we
       can always cache if there is a conflicting access. */
    CLPRINT("Message size: %u; free_region.size: %i;\n", size, free_region==NULL ? -1 : free_region->size);

    if (free_region!=NULL) Tree_DeleteNode(cache->free_idx, free_region->tnode);

    /* end find free region */
    TIMER_STOP(CL_AVL_SEARCH)

    /* sanity checks */
    assert(free_region==NULL || free_region->type==FREE);

    /* prepare stuff for possible eviction */
    cl_entry_t * victim = NULL;
    int htvictim = -1; // -1 if there is space in the HT, an index if there is a loop in the insertion path
    int ev_type;

    /* start from a random table. note: it has to be the same in insert_check and insert */
    int start_table = RAND() % CL_CUCKOO_TABLES;
    
    /* Priority to ht_insert_check to prevent the case in which: 
       1) there is no space
       2) there is a cl_entry_t (due to the space eviction) but the CL_CUCKOO_MAX_INSERT is reached
      In this way, if there is conflict in the HT I make the eviction to remove such conflict, then I check for
      the space. 
    */

    /* check if there is space in the HT */
    if ((htvictim = ht_insert_check(cache, target_rank, target_disp, size, start_table))>=0){    
        /* if we are here, then the HT was full or there was a conflict: we have a victim */
        CLPRINT("NO HT ENTRY! victim: %i\n", htvictim);

#ifdef CL_STATS
        cache->stat.ht_evictions++;
#endif

#ifdef CL_ADAPTIVE
        if (cl_adapt(win, 0)) {
            CLPRINT("Adaptive scheme triggered!\n-----------------\n\n");
            return CL_FLUSHED;
        }
#endif

        /* get the victim centry */
        victim = (cl_entry_t *) (TABLEPTR(cache) + htvictim);


        ev_type = CL_CACHED_R_HT; //eviction type */
    }else if (free_region==NULL){ 
        /* if we are here, then there was space in the HT but not in the buffer. Need to evict! */
        CLPRINT("NO SPACE!\n");

#ifdef CL_STATS
        cache->stat.sp_evictions++;
#endif

        /*==== Victim Selection (capacity miss) ===*/
        ev_type = CL_CACHED_R_SPACE;
        double minscore=0, cscore;
	    int ivictim=-1;
        cl_entry_t * curr;  

        int i, c;
        CLPRINT("Evicting: searching victim; starting from: %i\n", idx);

    
        /* victim selection (no space case): scan until you see CL_MAX_SCAN entries or, if they are empty entries, keep scanning unitl you find at least
           one non empty entry. Do not visit more than cache->table_size entries (otw you will loop). */
	    TIMER_START
        for (i=0, c=0; (i<CL_MAX_SCAN || c==0) && c<cache->table_size; idx = (idx + 1) % CL_HT_ENTRIES(cache), i++){
            
            //printf("i: %i; idx: %i; table[idx]: %i; c: %i\n", i, idx, cache->table[idx].index, c);
            /* .lasthit>0 if the entry is not empty */
            if (cache->table[idx].lasthit>0){
        
                /* get the entry with the minimum score */
                cscore = SCORE(cache->table[idx].lasthit, cache->table[idx].penalty);
                if (cscore<=minscore || c==0) { 
                    minscore=cscore; 
                    ivictim=cache->table[idx].index;
                }

                c++;
            }//else if (i % 10 == 0) idx = rand() % CL_HT_ENTRIES;
          
        }
	    TIMER_STOP(CL_VICTIM_SELECTION)

#ifdef CL_STATS
        cache->stat.last_eviction_scan = i;
        cache->stat.last_eviction_sample = c;
        cache->stat.tot_eviction_scan += i;
        cache->stat.tot_eviction_sample += c;
#endif   
    
#ifdef CL_ADAPTIVE
        if (cl_adapt(win, 0)) {
            CLPRINT("Adaptive scheme triggered!\n-----------------\n\n");
            return CL_FLUSHED;
        }
#endif
        assert(ivictim!=-1);
        victim = (cl_entry_t *) (TABLEPTR(cache) + ivictim);
    }

    CLPRINT("####victim: %p, victim==NULL: %i\n", victim, victim==NULL);

    if (victim!=NULL) {
    /* we have a victim (or due to HT conflict or due to space) */  

        cl_entry_t * new_free_region;

	    TIMER_START
#ifdef CL_STATS
        cache->stat.evicted_size += victim->get_info.size;
#endif

        CLPRINT("Evicting: victim found: %p; rank: %i; addr: %i; size: %i htindex: %i [%u %u]\n", victim, victim->get_info.rank, victim->get_info.addr, victim->get_info.size, victim->htindex, CLEFT(victim), CRIGHT(victim));


        /* change the victim descriptor to a free memory region descriptor */
        victim->size = victim->get_info.size;

        cl_entry_t *free=NULL, *next=NULL, *prev=NULL;

        new_free_region = victim;
        
        uint8_t index_new_free_region=1;
        /* Merging adjacent free regions */
        if (victim->mprev!=NOENTRY){

            /* get prev descriptor */
            prev = TABLEENTRY(cache, victim->mprev);

            /* if it's a free region, then we have to merge */
            if (prev->type==FREE){
                assert(prev->tnode!=NULL);
                CLPRINT("mergine prev: [%u %u]\n", CLEFT(prev), CRIGHT(prev));

                /* increase the size */
                prev->size += victim->size;

                /* remove from the index */
                /* the free_region has already been deleted from the tree. Do not delete again if
                   you are expanding that. */           
                if (prev!=free_region) Tree_DeleteNode(cache->free_idx, prev->tnode);
                else index_new_free_region = 0;
                /* set the new merged free region */
                new_free_region = prev;

                /* adjust the list: we are removing the victim descriptor due to the merge*/
                new_free_region->mnext = victim->mnext;        
                if (victim->mnext!=NOENTRY) TABLEENTRY(cache, victim->mnext)->mprev = new_free_region->index;

                /* free the victim descriptor */
                FREE_CENTRY(cache, victim);    

                /* prev must point to the prev non-empty entry */
                prev = prev->mprev!=NOENTRY ? TABLEENTRY(cache, prev->mprev) : NULL;                
                
            }            
        }


        if (new_free_region->mnext!=NOENTRY){
            
            /* get next descriptor */
            next = TABLEENTRY(cache, new_free_region->mnext);

            if (next->type==FREE){
                assert(next->tnode!=NULL);
                CLPRINT("mergine next: [%u %u]\n", CLEFT(next), CRIGHT(next));
            
                /* extend the free_region */
                new_free_region->size += next->size;
                
                /* remove from the index */
                /* the free_region has already been deleted from the tree. Do not delete again if
                   you are expanding that. */           
                if (next!=free_region) Tree_DeleteNode(cache->free_idx, next->tnode);
                else index_new_free_region = 0;
   
                /* adjust the list: we are removing the next descriptor due to the merge */
                new_free_region->mnext = next->mnext;
                if (next->mnext!=NOENTRY) TABLEENTRY(cache, next->mnext)->mprev = new_free_region->index;

                /* free this descriptor */
                FREE_CENTRY(cache, next);

                /* next must point to the next non-empty entry */
                next = next->mnext!=NOENTRY ? TABLEENTRY(cache, next->mnext) : NULL;                

            }
        }

        /* index new free region */
        new_free_region->type = FREE;

    
        /* We put the new entry in the newly created free region only if:
            1) no free_region was found before && the new_free_region is big 
               enough to contain the entry OR
            2) the previously selected free_region (from the tree) has been 
               expanded (i.e. !index_new_free_region).

           It covers the case in which there is space but not HT entries: you evict 
           an entry due to HT a free_region was already selected. If you blindly take 
           the new free region as destination it might be that this will not be big enough, 
           resulting in an erroneous capacity miss. */

        if ((free_region==NULL && new_free_region->size >= size) || !index_new_free_region) free_region = new_free_region;
        else if (index_new_free_region){
            /* we index the new_free_region only if this is not an expansion of the previously selected one. */
            CLPRINT("Indexing new free region [%u %u]. (free_region: %p, new_free_region->size: %u; size: %u)\n", CLEFT(new_free_region), CRIGHT(new_free_region), free_region, new_free_region->size, size);
            new_free_region->tnode = Tree_Insert(cache->free_idx, new_free_region);
            assert(new_free_region->tnode!=NULL);
        }
        /* prev | free | victim | free | next */
        /* prev | free | victim | free */
        /* victim | free | next */

        /* update penalties */
        if (prev!=NULL && next!=NULL){
            /* middle of the list */

            /* additional penalty for the previous and next elements */
            int ppen = next->offset - victim->offset;
            int npen = victim->offset + victim->size - (prev->offset + prev->get_info.size);

            /* update */
            HT(cache, prev).penalty += ppen;
            HT(cache, next).penalty += npen;
                     

        }else if (prev!=NULL){
            /* prev now is the last element, so remove its right penalty */
            HT(cache, prev).penalty -= (victim->offset - prev->offset + prev->get_info.size);

        }else if (next!=NULL){
            /* beginning of the list */
            HT(cache, next).penalty += victim->size;

        }//else {wtf?}


        /* ===eviction=== */

        /* remove it from the HT */
        ht_remove(cache, victim);            

        victim->pending=-1;

        TIMER_STOP(CL_EVICTION)

    } /* end eviction */

    if (free_region==NULL || free_region->size < size){
        CLPRINT("Not enough space. Cannot cache this entry!\n");
        CLPRINT("-----------------\n\n");

#ifdef CL_STATS
        cache->stat.failed++;
        return (ev_type == CL_CACHED_R_SPACE) ? CL_NOT_CACHED_SPACE : CL_NOT_CACHED_HT;
#else
        return CL_NOT_CACHED;
#endif

    }

    /*==== We found some space: allocate the new entry! ====*/
    CLPRINT("Found some space! free_region: %p (index: %i); size: %u [%u %u]\n", free_region, free_region->index, free_region->size, CLEFT(free_region), CRIGHT(free_region));

    TIMER_START

    cl_entry_t *newe, *prev, *next;

    /* remove free region from the index */

    if (free_region->size == size){
        newe = free_region;
    }else if (free_region->size > size){
        NEW_CENTRY(cache, newe);
        newe->mnext = free_region->index;
        if (free_region->mprev!=NOENTRY){
            TABLEENTRY(cache, free_region->mprev)->mnext = newe->index;
        }
        newe->mprev = free_region->mprev;
        free_region->mprev = newe->index;
        newe->offset = free_region->offset;
        free_region->offset += size;
        free_region->size -= size;
        free_region->tnode = Tree_Insert(cache->free_idx, free_region);
    }
  
    newe->type = BUSY;       


    CLPRINT("newe->mprev: %i; newe->mnext: %i\n", newe->mprev, newe->mnext);
    prev = (newe->mprev!=NOENTRY) ? TABLEENTRY(cache, newe->mprev) : NULL;
    next = (newe->mnext!=NOENTRY) ? TABLEENTRY(cache, newe->mnext) : NULL;

    CLPRINT("next->type: %i, new->offset: %i\n", (next!=NULL) ? next->type : -1, newe->offset);
    
    if (next!=NULL && next->type==FREE) next = (next->mnext!=NOENTRY) ? TABLEENTRY(cache, next->mnext) : NULL;
    
    /* add entry in the HT (it will also fill the remaining fields of newe) */
    ht_insert(cache, target_rank, target_disp, size, start_table, newe);

#ifdef CLDEBUG
    print_neigh(cache, newe);
    printf("\n");
#endif


    CLPRINT("Writing new entry at: [%u %u]\n", CLEFT(newe), CRIGHT(newe));
    /* Adjust penalties:
     | prev |   free     | next 
     now is:
     | prev | new | free | next

     prev.penalty -= (next.offset - new.offset)
     next.penalty -= size

     Note: if new is the last cache entry (i.e., | prev | new | free),    
    */

    /* can do this only after the ht_insert! */   
    HT(cache, newe).penalty = 0;

    uint32_t rightend;
    if (next!=NULL){
        HT(cache, next).penalty -= newe->get_info.size;
        HT(cache, newe).penalty = next->offset - (newe->offset + newe->get_info.size);
        rightend = next->offset;
    }else rightend = 0;


    
    if (prev!=NULL && rightend>0){
        HT(cache, prev).penalty -= (rightend - newe->offset);
    }


    /* Add pending copy */
    if (PEN_PTR(win, cache) >= PEN_TOP(win)) { 
        printf("Error too many pending copies (max: %i)\n", CL_PENDING_STACK);
        //return CL_FAIL;
        exit(-1);
    }else{
        cl_pending_t t;
        t.target_addr = (VOID *) MEMORY(cache) + newe->offset;
        t.origin_addr = origin_addr;


        newe->pending = PEN_PTR(win, cache);
        t.entry = newe->index;
        t.size = size;
        cache->pending[PEN_PTR(win, cache)++] = t;
    
        //CLPRINT("Adding pending copy from %p to %p;\n", NULL, t.target_addr);

    }

    CLPRINT("-----------------\n\n");

#ifdef CL_STATS
    cache->stat.cached++;
#endif

    TIMER_STOP(CL_CACHING);

    return (victim!=NULL) ? ev_type : CL_CACHED;
}

