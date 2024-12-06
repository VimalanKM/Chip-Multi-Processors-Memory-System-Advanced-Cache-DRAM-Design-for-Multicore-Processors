///////////////////////////////////////////////////////////////////////////////
// You will need to modify this file to implement part A and, for extra      //
// credit, parts E and F.                                                    //
///////////////////////////////////////////////////////////////////////////////

// cache.cpp
// Defines the functions used to implement the cache.

#include "cache.h"
#include <stdio.h>
#include <stdlib.h>
// You may add any other #include directives you need here, but make sure they
// compile on the reference machine!

///////////////////////////////////////////////////////////////////////////////
//                    EXTERNALLY DEFINED GLOBAL VARIABLES                    //
///////////////////////////////////////////////////////////////////////////////

/**
 * The current clock cycle number.
 * 
 * This can be used as a timestamp for implementing the LRU replacement policy.
 */
extern uint64_t current_cycle;

/**
 * For static way partitioning, the quota of ways in each set that can be
 * assigned to core 0.
 * 
 * The remaining number of ways is the quota for core 1.
 * 
 * This is used to implement extra credit part E.
 */
extern unsigned int SWP_CORE0_WAYS;

//Part F - Dynamic Way Partitioning Parameters
uint64_t DWP_CORE0_WAYS = 0;
uint64_t DWP_CORE1_WAYS = 0;
uint64_t DWP_SWITCH = 0;
float MISS_RATE_CORE_0 = 0;
float MISS_RATE_CORE_1 = 0;

///////////////////////////////////////////////////////////////////////////////
//                           FUNCTION DEFINITIONS                            //
///////////////////////////////////////////////////////////////////////////////

// As described in cache.h, you are free to deviate from the suggested
// implementation as you see fit.

// The only restriction is that you must not remove cache_print_stats() or
// modify its output format, since its output will be used for grading.

/**
 * Allocate and initialize a cache.
 * 
 * This is intended to be implemented in part A.
 *
 * @param size The size of the cache in bytes.
 * @param associativity The associativity of the cache.
 * @param line_size The size of a cache line in bytes.
 * @param replacement_policy The replacement policy of the cache.
 * @return A pointer to the cache.
 */
Cache *cache_new(uint64_t size, uint64_t associativity, uint64_t line_size,
                 ReplacementPolicy replacement_policy)
{
    // TODO: Allocate memory to the data structures and initialize the required
    //       fields. (You might want to use calloc() for this.)
    Cache *cache = (Cache*)calloc(1, sizeof(Cache));
    cache->number_of_ways = associativity; //number of lines under each set
    cache->number_of_sets = size / (line_size * associativity); //number of sets
    cache->line_size = line_size;
    cache->replacement_policy = replacement_policy;

    #ifdef DEBUG
        printf("Creating cache (# sets: %d, # ways: %d)\n", (int)cache->number_of_sets, (int)cache->number_of_ways);
    #endif

    //Memory allocation for each set
    cache->sets = (CacheSet*)calloc(cache->number_of_sets, sizeof(CacheSet));

    //stats
    cache->stat_read_access = 0;
    cache->stat_read_miss = 0;
    cache->stat_write_access = 0;
    cache->stat_write_miss = 0;
    cache->stat_dirty_evicts = 0;

    //Initializing the set and its contents
    for(uint64_t i=0; i<cache->number_of_sets; i++){
        cache->sets[i].lines = (CacheLine*)calloc(cache->number_of_ways, sizeof(CacheLine));
        for(uint64_t j=0; j<cache->number_of_ways; j++){
            cache->sets[i].lines[j].valid = false;
            cache->sets[i].lines[j].dirty = false;
        }
    }

    return cache;

}

uint64_t extract_index(uint64_t line_addr, int index_bits) {
    return line_addr & ((1ULL << index_bits) - 1);
}

uint64_t extract_tag(uint64_t line_addr, int index_bits) {
    return line_addr >> index_bits;
}

/**
 * Access the cache at the given address.
 * 
 * Also update the cache statistics accordingly.
 * 
 * This is intended to be implemented in part A.
 * 
 * @param c The cache to access.
 * @param line_addr The address of the cache line to access (in units of the
 *                  cache line size, i.e., excluding the line offset bits).
 * @param is_write Whether this access is a write.
 * @param core_id The CPU core ID that requested this access.
 * @return Whether the cache access was a hit or a miss.
 */
CacheResult cache_access(Cache *c, uint64_t line_addr, bool is_write,
                         unsigned int core_id)
{
    // TODO: Return HIT if the access hits in the cache, and MISS otherwise.
    // TODO: If is_write is true, mark the resident line as dirty.
    // TODO: Update the appropriate cache statistics.

    //uint64_t set_index = line_addr % c->number_of_sets;
    //uint64_t tag = line_addr / (c->line_size * c->number_of_sets);
    uint64_t set_index = extract_index(line_addr, __builtin_log2(c->number_of_sets));
    uint64_t tag = extract_tag(line_addr, __builtin_log2(c->number_of_sets));

    #ifdef DEBUG
        printf("\t\tindex: %ld, tag: %ld, is_write: %d, core_id: %d\n", set_index, tag, is_write, core_id);
    #endif

    CacheSet* set = &c->sets[set_index];

    if(is_write){
        c->stat_write_access++;
    }
    else{
        c->stat_read_access++;
    }

    for(uint64_t i=0; i<c->number_of_ways; i++){
        CacheLine* line = &set->lines[i];
        if(line->valid && line->tag == tag){
            //Cache Hit
            #ifdef DEBUG
                printf("\t\tHit in the cache --> is_write: %d\n", is_write);
            #endif

            if(is_write){
                line->dirty = true;
            }
            
            line->last_access_time = current_cycle;
            

            return HIT;
        }
    }

    #ifdef DEBUG
        printf("\t\tMISS!\n");
    #endif

    // c->stat_read_access += !is_write;
    // c->stat_write_access += is_write;
    if(is_write){
        c->stat_write_miss++;
    }
    else{
        c->stat_read_miss++;
    }
    // c->stat_read_miss += !is_write;
    // c->stat_write_miss += is_write;

    //cache_install(c, line_addr, is_write, core_id);
    return MISS;
}

/**
 * Install the cache line with the given address.
 * 
 * Also update the cache statistics accordingly.
 * 
 * This is intended to be implemented in part A.
 * 
 * @param c The cache to install the line into.
 * @param line_addr The address of the cache line to install (in units of the
 *                  cache line size, i.e., excluding the line offset bits).
 * @param is_write Whether this install is triggered by a write.
 * @param core_id The CPU core ID that requested this access.
 */
void cache_install(Cache *c, uint64_t line_addr, bool is_write,
                   unsigned int core_id)
{
    // TODO: Use cache_find_victim() to determine the victim line to evict.
    // TODO: Copy it into a last_evicted_line field in the cache in order to
    //       track writebacks.
    // TODO: Initialize the victim entry with the line to install.
    // TODO: Update the appropriate cache statistics.

    uint64_t set_index = extract_index(line_addr, __builtin_log2(c->number_of_sets));
    uint64_t tag = extract_tag(line_addr, __builtin_log2(c->number_of_sets));

    #ifdef DEBUG
        printf("\t\tInstalling into a cache (index: %ld)\n", set_index);
    #endif

    CacheSet* set = &c->sets[set_index];

    unsigned int victim_index = cache_find_victim(c, set_index, core_id);
    CacheLine* victim_line = &set->lines[victim_index];

    if(victim_line->valid && victim_line->dirty){
        c->stat_dirty_evicts++;
        #ifdef DEBUG
            printf("\t\tVictim was dirty!\n");
        #endif
    }

    c->last_evicted_line = *victim_line;

    victim_line->valid = true;
    victim_line->tag = tag;
    victim_line->core_id = core_id;
    victim_line->dirty = is_write;
    victim_line->last_access_time = current_cycle;

    

    #ifdef DEBUG
        printf("\t\tNew cache line installed (dirty: %d, tag: %ld, core_id: %d, last_access_time: %ld)\n", victim_line->dirty, 
        victim_line->tag, victim_line->core_id, victim_line->last_access_time);
    #endif
}

/**
 * Find which way in a given cache set to replace when a new cache line needs
 * to be installed. This should be chosen according to the cache's replacement
 * policy.
 * 
 * The returned victim can be valid (non-empty), in which case the calling
 * function is responsible for evicting the cache line from that victim way.
 * 
 * This is intended to be initially implemented in part A and, for extra
 * credit, extended in parts E and F.
 * 
 * @param c The cache to search.
 * @param set_index The index of the cache set to search.
 * @param core_id The CPU core ID that requested this access.
 * @return The index of the victim way.
 */
unsigned int cache_find_victim(Cache *c, unsigned int set_index,
                               unsigned int core_id)
{
    // TODO: Find a victim way in the given cache set according to the cache's
    //       replacement policy.
    // TODO: In part A, implement the LRU and random replacement policies.
    // TODO: In part E, for extra credit, implement static way partitioning.
    // TODO: In part F, for extra credit, implement dynamic way partitioning.

    #ifdef DEBUG
        printf("\t\tLooking for victim to evict (policy: %d)...\n", c->replacement_policy);
    #endif

    CacheSet* set = &c->sets[set_index];

    uint64_t ways_taken_by_core0 = 0;
    uint64_t ways_taken_by_core1 = 0;
    uint64_t SWP_CORE1_WAYS = c->number_of_ways - SWP_CORE0_WAYS;

    uint64_t oldest_time_core0 = UINT64_MAX;
    uint64_t oldest_time_core1 = UINT64_MAX;
    uint64_t victim_index_core0 = 0;
    uint64_t victim_index_core1 = 0;

    //DWP - Finding Algorithm
    if(core_id == 0){
        MISS_RATE_CORE_0 = (c->stat_read_miss + c->stat_write_miss)/(c->stat_read_access + c->stat_write_access) - MISS_RATE_CORE_0;
    }
    else{
        MISS_RATE_CORE_1 = (c->stat_read_miss + c->stat_write_miss)/(c->stat_read_access + c->stat_write_access) - MISS_RATE_CORE_1;
    }

    //DWP - switching algorithm
    if(MISS_RATE_CORE_0 > MISS_RATE_CORE_1){
        DWP_SWITCH = 1;
        
        DWP_CORE0_WAYS = SWP_CORE0_WAYS + 6;
        DWP_CORE1_WAYS = c->number_of_ways - DWP_CORE0_WAYS;
    }
    else if(MISS_RATE_CORE_1 > MISS_RATE_CORE_0){
        DWP_SWITCH = 0;
        
        DWP_CORE1_WAYS = SWP_CORE0_WAYS + 6;
        DWP_CORE0_WAYS = c->number_of_ways - DWP_CORE1_WAYS;
    }

    //checking for space in the given index
    for(uint64_t i=0; i<c->number_of_ways; i++){
        if(!set->lines[i].valid){
            
            #ifdef DEBUG
                printf("\t\tFound a naive victim (valid bit not set, idx: %d)\n", (int)i);
            #endif

            return i;
        }   

        //when the line is not naive
        //finding the ways occupied by each core
        if(set->lines[i].core_id == 0){
            ways_taken_by_core0++;
        }
        else{
            ways_taken_by_core1++;
        }

        //finding LRU of core 0 and core 1
        if(set->lines[i].last_access_time < oldest_time_core0 && set->lines[i].core_id == 0){
            oldest_time_core0 = set->lines[i].last_access_time;
            victim_index_core0 = i;
        }
        if(set->lines[i].last_access_time < oldest_time_core1 && set->lines[i].core_id == 1){
            oldest_time_core1 = set->lines[i].last_access_time;
            victim_index_core1 = i;
        }
        
    }

    //All lines have been found to be valid
    //Have to find the line to be replaced based on the policy
    uint64_t victim_index = 0;

    if(c->replacement_policy == LRU){
        uint64_t oldest_time = set->lines[0].last_access_time;
        for(unsigned int i=1; i<c->number_of_ways; i++){
            if(set->lines[i].last_access_time < oldest_time){
                oldest_time = set->lines[i].last_access_time;
                victim_index = i;
            }
        }
    }
    else if(c->replacement_policy == RANDOM){
        victim_index =  rand() % c->number_of_ways;
    }
    
    /*
    static way partitoning:
    if you have cache quota remaining in your core: use it
    if you have cache quota full in your core and have space in other core: use the other core
    if both cores are full: Use LRU to put data in your core
    */
    else if(c->replacement_policy == SWP){
        if(core_id == 0){
            //checking if core 1 has occupied more than its quota
            if(ways_taken_by_core1 > SWP_CORE1_WAYS){
                victim_index = victim_index_core1;
            }
            else{
                victim_index = victim_index_core0;
            }
        }
        if(core_id == 1){
            //checking if core 1 has occupied more than its quota
            if(ways_taken_by_core0 > SWP_CORE0_WAYS){
                victim_index = victim_index_core0;
            }
            else{
                victim_index = victim_index_core1;
            }
        }
    }

    //dynamic way partitioning
    else if(c->replacement_policy == DWP){
        if(core_id == 0){
            //checking if core 1 has occupied more than its quota
            if(ways_taken_by_core1 > DWP_CORE1_WAYS){
                victim_index = victim_index_core1;
            }
            else{
                victim_index = victim_index_core0;
            } 
        }

        if(core_id == 1){
            //checking if core 1 has occupied more than its quota
            if(ways_taken_by_core0 > DWP_CORE0_WAYS){
                victim_index = victim_index_core0;
            }
            else{
                victim_index = victim_index_core1;
            }
        }

    }

    #ifdef DEBUG
        printf("\t\tFound a victim (policy_num: %d, idx: %ld)\n", c->replacement_policy, victim_index);
    #endif

    return victim_index;
}


/**
 * Print the statistics of the given cache.
 * 
 * This is implemented for you. You must not modify its output format.
 * 
 * @param c The cache to print the statistics of.
 * @param label A label for the cache, which is used as a prefix for each
 *              statistic.
 */
void cache_print_stats(Cache *c, const char *header)
{
    double read_miss_percent = 0.0;
    double write_miss_percent = 0.0;

    if (c->stat_read_access)
    {
        read_miss_percent = 100.0 * (double)(c->stat_read_miss) /
                            (double)(c->stat_read_access);
    }

    if (c->stat_write_access)
    {
        write_miss_percent = 100.0 * (double)(c->stat_write_miss) /
                             (double)(c->stat_write_access);
    }

    printf("\n");
    printf("%s_READ_ACCESS     \t\t : %10llu\n", header, c->stat_read_access);
    printf("%s_WRITE_ACCESS    \t\t : %10llu\n", header, c->stat_write_access);
    printf("%s_READ_MISS       \t\t : %10llu\n", header, c->stat_read_miss);
    printf("%s_WRITE_MISS      \t\t : %10llu\n", header, c->stat_write_miss);
    printf("%s_READ_MISS_PERC  \t\t : %10.3f\n", header, read_miss_percent);
    printf("%s_WRITE_MISS_PERC \t\t : %10.3f\n", header, write_miss_percent);
    printf("%s_DIRTY_EVICTS    \t\t : %10llu\n", header, c->stat_dirty_evicts);
}
