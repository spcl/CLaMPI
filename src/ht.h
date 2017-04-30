#ifndef __CLAMPI_HT__
#define __CLAMPI_HT__

#include "clampi_internal.h"

#define CLKEY1
#ifdef CLKEY1
 #define HT_KEY(rank, displ, count) rank*100 + displ*10 + count*1
#else
 #define HT_KEY(rank, displ, count) (rank ^ displ) ^ count
#endif


#define CUCKOO_HASHING
#ifdef CUCKOO_HASHING
  #define CL_CUCKOO_TABLES 4
  #define CL_CUCKOO_TABLE_SIZE(SIZE) (SIZE/CL_CUCKOO_TABLES)                              
  #define CL_CUCKOO_MAX_INSERT 10
#endif


#define HT_ENTRIES_PER_CACHELINE (CACHE_LINE / sizeof(cl_index_t))


#ifdef CHAIN_HASHING 
 #define CMPI_HASH(o, res, htsize) \
 { \
    int x = o; \
    x = ((x >> 16) ^ x) * 0x45d9f3b;\
    x = ((x >> 16) ^ x) * 0x45d9f3b; \
    x = ((x >> 16) ^ x); \
    res = x % htsize; \
 }
#elif defined(CUCKOO_HASHING)
 #define CMPI_HASH(table, o, res, htsize) \
 { \
    int a[] = {33, 5, 51, 19}; \
    uint64_t h = 5381; \
    h = (h*a[table] + o.rank);  \
    h = (h*a[table] + o.addr); \
    h = (h*a[table] + o.size); \
    res = h % CL_CUCKOO_TABLE_SIZE(htsize); \
 }
#endif



static inline int ht_insert_check(cl_cache_t * cache, int rank, int addr, int size, int table){
    int hkey;
    //int key = HT_KEY(rank, addr, size);
    int count = 0;
    cl_index_t victim, newvictim;  
    
    cl_get_t key;
    key.rank=rank;
    key.addr=addr;
    key.size=size;

    CMPI_HASH(table, key, hkey, CL_HT_ENTRIES(cache));

    //printf("ht_insert_check: rank: %i, addr: %i, size: %i; hkey: %i\n", rank, addr, size, hkey);
    
    int toevict = -1;
    double minscore,cscore;    

    victim = cache->table[hkey*CL_CUCKOO_TABLES + table];
    if (victim.lasthit>0) {
        //score
        minscore = SCORE(victim.lasthit, victim.penalty);
        toevict = victim.index;
        count++;
        table = (table + 1) % CL_CUCKOO_TABLES;
        CMPI_HASH(table, key, hkey, CL_HT_ENTRIES(cache));

        //fprintf(stderr, "hkey: %i; table: %i\n", hkey, table);
        victim = cache->table[hkey*CL_CUCKOO_TABLES + table];  
    }

    while (victim.lasthit>0 && count<=CL_CUCKOO_MAX_INSERT){
        cl_entry_t * rvictim = TABLEPTR(cache) + victim.index;
        //compute score (for the potential eviciton)
        cscore = SCORE(victim.lasthit, victim.penalty);

        //printf("cuckoo getcount: %i\n", cache->getcount);
        //CLPRINT("Cuckoo check: current index: %i: count: %i; CL_CLUCKOO_MAX_INSERT: %i\n", victim.index, count, CL_CUCKOO_MAX_INSERT);
        if (count==0 || cscore < minscore){
            toevict = victim.index;
            minscore = cscore;
        }

        table = (table + 1) % CL_CUCKOO_TABLES;
        //key = HT_KEY(rvictim->get_info.rank, rvictim->get_info.addr, rvictim->get_info.size);
        CMPI_HASH(table, rvictim->get_info, hkey, CL_HT_ENTRIES(cache));
        /*select new victim */
        victim = cache->table[hkey*CL_CUCKOO_TABLES + table];
        count++;
    }

#ifdef CL_STATS
    cache->stat.last_ht_scan = count-1;
#endif

    CLPRINT("count: %i; max: %i; toevict: %i\n", count, CL_CUCKOO_MAX_INSERT, toevict);

    return (victim.lasthit>0) ? toevict : -1; 
}




static inline int ht_insert(cl_cache_t * cache, int rank, int addr, int size, int table, cl_entry_t * entry){
    int hkey;

    //int key = HT_KEY(rank, addr, size);
    int count = 0;
    //int table = 0; //TODO: this has to be randomly selected
    cl_index_t victim, newvictim;  
    

    //table = rand() % 2; // ht_insert_check(cache, rank, addr, size);
    //if (table<0) return NULL;
    CLPRINT("HT_INSERT; cache->next_free_entry: %i; %p\n", cache->next_free_entry, &cache->free_entries[cache->next_free_entry]);

    
    cache->table_size++; 

    cl_get_t newget;
    newget.rank=rank;
    newget.addr=addr;
    newget.size=size;

    CMPI_HASH(table, newget, hkey, CL_HT_ENTRIES(cache));
    /* first swap */
    
    victim = cache->table[hkey*CL_CUCKOO_TABLES + table];
    if (victim.lasthit>0) {
        count++;
        table = (table+1) % CL_CUCKOO_TABLES;
        CMPI_HASH(table, newget, hkey, CL_HT_ENTRIES(cache));
        victim = cache->table[hkey*CL_CUCKOO_TABLES + table];  
    }

    cache->table[hkey*CL_CUCKOO_TABLES + table].index = entry->index;

    entry->get_info = newget;
    entry->htindex = hkey*CL_CUCKOO_TABLES + table;  


    cache->table[entry->htindex].lasthit = cache->getcount;
 
    while (victim.lasthit>0 && count<=CL_CUCKOO_MAX_INSERT){
        cl_entry_t * rvictim = TABLEPTR(cache) + victim.index;


        table = (table+1) % CL_CUCKOO_TABLES;
        
        //key = HT_KEY(rvictim->get_info.rank, rvictim->get_info.addr, rvictim->get_info.size);
        CMPI_HASH(table, rvictim->get_info, hkey, CL_HT_ENTRIES(cache));

        /*select new victim */
        newvictim = cache->table[hkey*CL_CUCKOO_TABLES + table];
        
        cache->table[hkey*CL_CUCKOO_TABLES + table] = victim;
        victim = newvictim;
        rvictim->htindex = hkey*CL_CUCKOO_TABLES + table;
        count++;
    }

#ifdef CLDEBUG  
/*
    if (count>CL_CUCKOO_MAX_INSERT) {
        printf("CUCKOO MAX INSERT!!!\n");
        exit(-1);
    }
*/
#endif

    return 0;    
}

static cl_entry_t * ht_lookup(cl_cache_t * cache, int rank, int addr, int size){

    int hkey;
    //int key = HT_KEY(rank, addr, size);

    cl_get_t key;
    key.rank = rank;
    key.addr = addr;
    key.size = size;
    for (int i=0; i<CL_CUCKOO_TABLES; i++){
        CMPI_HASH(i, key, hkey, CL_HT_ENTRIES(cache));
        //printf("hkey: %i\n", hkey);
        if (cache->table[hkey*CL_CUCKOO_TABLES + i].lasthit==0) continue;

        //printf("CUCKOO looking at: %i\n", hkey*CL_CUCKOO_TABLES + i);
        cl_entry_t * c = TABLEPTR(cache) + cache->table[hkey*CL_CUCKOO_TABLES + i].index;
        if (c->get_info.rank==rank && c->get_info.addr==addr && c->get_info.size==size) {
            CLPRINT("ht_lookup ok: rank: %i; addr: %i; size: %i\n", c->get_info.rank, c->get_info.addr, c->get_info.size);
            cache->table[hkey*CL_CUCKOO_TABLES + i].lasthit = cache->getcount;
            return c;
        }
    }

    //printf("lookup: fail\n");
    return NULL;   
}

static int ht_remove(cl_cache_t * cache, cl_entry_t * c){

#ifdef CLDEBUG
    if (cache->table[c->htindex].lasthit == 0){
        printf("Error: not indexed!\n");
        exit(-1);
    }
#endif


    //cache->free_entries[++cache->next_free_entry] = c->index;
    cache->table[c->htindex].lasthit = 0;
    cache->table_size--;
    return CL_SUCCESS;
    
}


#endif
