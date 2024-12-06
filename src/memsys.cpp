///////////////////////////////////////////////////////////////////////////////
// You will need to modify this file to implement parts B through F.         //
//                                                                           //
// In part B, it is intended that you implement the following functions:     //
// - memsys_access_modeBC() (used in parts B and C)                          //
// - memsys_l2_access() (used in parts B through F)                          //
//                                                                           //
// In part D, it is intended that you implement the following function:      //
// - memsys_access_modeDEF() (used in parts D, E, and F)                     //
///////////////////////////////////////////////////////////////////////////////

// memsys.cpp
// Defines the functions for the memory system.

#include "memsys.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
// You may add any other #include directives you need here, but make sure they
// compile on the reference machine!

///////////////////////////////////////////////////////////////////////////////
//                                 CONSTANTS                                 //
///////////////////////////////////////////////////////////////////////////////

/** The number of bytes in a page. */
#define PAGE_SIZE 4096

/** The hit time of the data cache in cycles. */
#define DCACHE_HIT_LATENCY 1

/** The hit time of the instruction cache in cycles. */
#define ICACHE_HIT_LATENCY 1

/** The hit time of the L2 cache in cycles. */
#define L2CACHE_HIT_LATENCY 10

///////////////////////////////////////////////////////////////////////////////
//                    EXTERNALLY DEFINED GLOBAL VARIABLES                    //
///////////////////////////////////////////////////////////////////////////////

/**
 * The current mode under which the simulation is running, corresponding to
 * which part of the lab is being evaluated.
 */
extern Mode SIM_MODE;

/** The number of bytes in a cache line. */
extern uint64_t CACHE_LINESIZE;

/** The replacement policy to use for the L1 data and instruction caches. */
extern ReplacementPolicy REPL_POLICY;

/** The size of the data cache in bytes. */
extern uint64_t DCACHE_SIZE;

/** The associativity of the data cache. */
extern uint64_t DCACHE_ASSOC;

/** The size of the instruction cache in bytes. */
extern uint64_t ICACHE_SIZE;

/** The associativity of the instruction cache. */
extern uint64_t ICACHE_ASSOC;

/** The size of the L2 cache in bytes. */
extern uint64_t L2CACHE_SIZE;

/** The associativity of the L2 cache. */
extern uint64_t L2CACHE_ASSOC;

/** The replacement policy to use for the L2 cache. */
extern ReplacementPolicy L2CACHE_REPL;

/** The number of cores being simulated. */
extern unsigned int NUM_CORES;

#define DELAY_SIM_MODE_B 100;

/**
 * The current clock cycle number.
 * 
 * This can be used as a timestamp for implementing the LRU replacement policy.
 */
extern uint64_t current_cycle;

///////////////////////////////////////////////////////////////////////////////
//                           FUNCTION DEFINITIONS                            //
///////////////////////////////////////////////////////////////////////////////

//additional functions
uint64_t extract_index_mem(uint64_t line_addr, int index_bits) {
    return line_addr & ((1ULL << index_bits)-1);
}

uint64_t extract_tag_mem(uint64_t line_addr, int index_bits) {
    return line_addr >> index_bits;
}

uint64_t find_line_address_from_tag_index_mem(uint64_t tag, uint64_t index, uint64_t index_bits) {
    // Shift the tag to the left to place it in its correct position in the address
    uint64_t line_address = tag << index_bits;

    // Combine the shifted tag with the index
    line_address |= index;

    return line_address;
}


/**
 * Allocate and initialize the memory system.
 * 
 * This is implemented for you, but you may modify it as needed.
 * 
 * @return A pointer to the memory system.
 */
MemorySystem *memsys_new()
{
    MemorySystem *sys = (MemorySystem *)calloc(1, sizeof(MemorySystem));

    if (SIM_MODE == SIM_MODE_A)
    {
        sys->dcache = cache_new(DCACHE_SIZE, DCACHE_ASSOC, CACHE_LINESIZE,
                                REPL_POLICY);
    }

    if (SIM_MODE == SIM_MODE_B || SIM_MODE == SIM_MODE_C)
    {
        sys->dcache = cache_new(DCACHE_SIZE, DCACHE_ASSOC, CACHE_LINESIZE,
                                REPL_POLICY);
        sys->icache = cache_new(ICACHE_SIZE, ICACHE_ASSOC, CACHE_LINESIZE,
                                REPL_POLICY);
        sys->l2cache = cache_new(L2CACHE_SIZE, L2CACHE_ASSOC, CACHE_LINESIZE,
                                 REPL_POLICY);
        sys->dram = dram_new();
    }

    if (SIM_MODE == SIM_MODE_DEF)
    {
        sys->l2cache = cache_new(L2CACHE_SIZE, L2CACHE_ASSOC, CACHE_LINESIZE,
                                 L2CACHE_REPL);
        sys->dram = dram_new();
        for (unsigned int i = 0; i < NUM_CORES; i++)
        {
            sys->dcache_coreid[i] = cache_new(DCACHE_SIZE, DCACHE_ASSOC,
                                              CACHE_LINESIZE, REPL_POLICY);
            sys->icache_coreid[i] = cache_new(ICACHE_SIZE, ICACHE_ASSOC,
                                              CACHE_LINESIZE, REPL_POLICY);
        }
    }

    return sys;
}

/**
 * Access the given memory address from an instruction fetch or load/store.
 * 
 * Return the delay in cycles incurred by this memory access. Also update the
 * statistics accordingly.
 * 
 * This is implemented for you, but you may modify it as needed.
 * 
 * @param sys The memory system to use for the access.
 * @param addr The address to access (in bytes).
 * @param type The type of memory access.
 * @param core_id The CPU core ID that requested this access.
 * @return The delay in cycles incurred by this memory access.
 */
uint64_t memsys_access(MemorySystem *sys, uint64_t addr, AccessType type,
                       unsigned int core_id)
{
    uint64_t delay = 0;

    // All cache transactions happen at line granularity, so we convert the
    // byte address to a cache line address.
    uint64_t line_addr = addr / CACHE_LINESIZE;

    if (SIM_MODE == SIM_MODE_A)
    {
        delay = memsys_access_modeA(sys, line_addr, type, core_id);
    }

    if (SIM_MODE == SIM_MODE_B || SIM_MODE == SIM_MODE_C)
    {
        delay = memsys_access_modeBC(sys, line_addr, type, core_id);
    }

    if (SIM_MODE == SIM_MODE_DEF)
    {
        delay = memsys_access_modeDEF(sys, line_addr, type, core_id);
    }

    // Update the statistics.
    if (type == ACCESS_TYPE_IFETCH)
    {
        sys->stat_ifetch_access++;
        sys->stat_ifetch_delay += delay;
    }

    if (type == ACCESS_TYPE_LOAD)
    {
        sys->stat_load_access++;
        sys->stat_load_delay += delay;
    }

    if (type == ACCESS_TYPE_STORE)
    {
        sys->stat_store_access++;
        sys->stat_store_delay += delay;
    }

    return delay;
}

/**
 * In mode A, access the given memory address from a load or store.
 * 
 * Return the simulated delay in cycles for this memory access, which is 0
 * because timing is not simulated in this mode.
 * 
 * This is implemented for you, but you may modify it as needed.
 * 
 * @param sys The memory system to use for the access.
 * @param line_addr The address of the cache line to access (in units of the
 *                  cache line size, i.e., excluding the line offset bits).
 * @param type The type of memory access.
 * @param core_id The CPU core ID that requested this access.
 * @return Always 0 in this mode.
 */
uint64_t memsys_access_modeA(MemorySystem *sys, uint64_t line_addr,
                             AccessType type, unsigned int core_id)
{
    bool needs_dcache_access = false;
    bool is_write = false;

    if (type == ACCESS_TYPE_IFETCH)
    {
        // Ignore instruction fetches because there is no instruction cache in
        // this mode.
    }

    if (type == ACCESS_TYPE_LOAD)
    {
        needs_dcache_access = true;
        is_write = false;
    }

    if (type == ACCESS_TYPE_STORE)
    {
        needs_dcache_access = true;
        is_write = true;
    }

    if (needs_dcache_access)
    {
        CacheResult outcome = cache_access(sys->dcache, line_addr, is_write,
                                           core_id);
        if (outcome == MISS)
        {
            cache_install(sys->dcache, line_addr, is_write, core_id);
        }
    }

    // Timing is not simulated in Part A.
    return 0;
}

/**
 * In mode B or C, access the given memory address from an instruction fetch or
 * load/store.
 * 
 * Return the delay in cycles incurred by this memory access. It is intended
 * that the calling function will be responsible for updating the statistics
 * accordingly.
 * 
 * This is intended to be implemented in part B.
 * 
 * @param sys The memory system to use for the access.
 * @param line_addr The address of the cache line to access (in units of the
 *                  cache line size, i.e., excluding the line offset bits).
 * @param type The type of memory access.
 * @param core_id The CPU core ID that requested this access.
 * @return The delay in cycles incurred by this memory access.
 */
uint64_t memsys_access_modeBC(MemorySystem *sys, uint64_t line_addr,
                              AccessType type, unsigned int core_id)
{
    uint64_t delay = 0;
    CacheResult l1_output;
    #ifdef DEBUG
        printf("\nAccessing memory in mode BC (line_addr: %ld, AccessType: %d, core_id: %d)\n", line_addr, type, core_id);
    #endif

    #ifdef DEBUG
        printf("\tAccessing L1 cache!\n");
    #endif

    if (type == ACCESS_TYPE_IFETCH)
    {
        // TODO: Simulate the instruction fetch and update delay accordingly.
        l1_output = cache_access(sys->icache, line_addr, false, core_id);
        delay += ICACHE_HIT_LATENCY;

        if (l1_output == MISS){
            //accessing L2 cache
            delay += memsys_l2_access(sys, line_addr, false, core_id);

            #ifdef DEBUG
                printf("\tInstalling line in L1 cache!\n");
            #endif
            cache_install(sys->icache, line_addr, false, core_id);
        }
    }
    //if(type == ACCESS_TYPE_LOAD || type == ACCESS_TYPE_STORE)
    else{
        bool is_write = (type == ACCESS_TYPE_STORE);
        l1_output = cache_access(sys->dcache, line_addr, is_write, core_id);
        delay += DCACHE_HIT_LATENCY;

        if(l1_output == MISS){
            delay += memsys_l2_access(sys, line_addr, false, core_id);

            #ifdef DEBUG
                printf("\tInstalling line in L1 cache!\n");
            #endif
            cache_install(sys->dcache, line_addr, is_write, core_id);
        }

        //Write back to L2 cache - Using Victim Line
        if(sys->dcache->last_evicted_line.valid && sys->dcache->last_evicted_line.dirty && !l1_output){
            uint64_t index_bits = __builtin_log2(sys->dcache->number_of_sets);
            uint64_t ind = extract_index_mem(line_addr, index_bits);
            uint64_t evicted_line_address = find_line_address_from_tag_index_mem(sys->dcache->last_evicted_line.tag, ind, index_bits);

            #ifdef DEBUG
                //printf("\tline addr: %ld, tag: %ld, index: %ld\n", line_addr, sys->dcache->last_evicted_line.tag, ind);
                printf("\tEvicted L1 entry was dirty! Performing writeback (addr: %ld)\n", evicted_line_address);
            #endif

            //cache_install(sys->l2cache, evicted_line_address, sys->dcache->last_evicted_line.dirty, core_id);
            memsys_l2_access(sys, evicted_line_address, sys->dcache->last_evicted_line.dirty, core_id);
        }
    }
    
    
    /*
    if (type == ACCESS_TYPE_LOAD)
    {
        // TODO: Simulate the data load and update delay accordingly.
        CacheResult l1_output = cache_access(sys->dcache, line_addr, false, core_id);

        delay += DCACHE_HIT_LATENCY;

        if(l1_output == MISS){
            delay += memsys_l2_access(sys, line_addr, false, core_id);
        }
    }

    if (type == ACCESS_TYPE_STORE)
    {
        // TODO: Simulate the data store and update delay accordingly.
    }
    */

    return delay;
}

/**
 * Access the given address through the shared L2 cache.
 * 
 * Return the delay in cycles incurred by the L2 (and possibly DRAM) access.
 * 
 * This is intended to be implemented in part B and used in parts B through F
 * for icache misses, dcache misses, and dcache writebacks.
 * 
 * @param sys The memory system to use for the access.
 * @param line_addr The (physical) address of the cache line to access (in
 *                  units of the cache line size, i.e., excluding the line
 *                  offset bits).
 * @param is_writeback Whether this access is a writeback from an L1 cache.
 * @param core_id The CPU core ID that requested this access.
 * @return The delay in cycles incurred by this access.
 */
uint64_t memsys_l2_access(MemorySystem *sys, uint64_t line_addr,
                          bool is_writeback, unsigned int core_id)
{
    uint64_t delay = L2CACHE_HIT_LATENCY;

    #ifdef DEBUG
        printf("\tAccessing L2 cache!\n");
    #endif

    // TODO: Perform the L2 cache access.

    // TODO: Use the dram_access() function to get the delay of an L2 miss.
    // TODO: Use the dram_access() function to perform writebacks to memory.
    //       Note that writebacks are done off the critical path.
    // This will help us track your memory reads and memory writes.

    //Accessing L2 cache
    CacheResult l2_output = cache_access(sys->l2cache, line_addr, is_writeback, core_id);

    if(l2_output == MISS){
        //when L2 misses, DRAM is accessed
        delay += dram_access(sys->dram, line_addr, false);

        //installing the new line in L2 cache
        #ifdef DEBUG
            printf("\tInstalling line in L2 cache!\n");
        #endif
        cache_install(sys->l2cache, line_addr, is_writeback, core_id);
    }

    if(sys->l2cache->last_evicted_line.valid && sys->l2cache->last_evicted_line.dirty && !l2_output){
        uint64_t index_bits = __builtin_log2(sys->l2cache->number_of_sets);
        uint64_t ind = extract_index_mem(line_addr, index_bits);
        uint64_t evicted_line_address = find_line_address_from_tag_index_mem(sys->l2cache->last_evicted_line.tag, ind, index_bits);

        #ifdef DEBUG
            printf("\tEvicted L2 entry was dirty! Performing writeback (addr: %ld)\n", evicted_line_address);
        #endif

        dram_access(sys->dram, evicted_line_address, sys->l2cache->last_evicted_line.dirty);
    }

    return delay;
}

/**
 * In mode D, E, or F, access the given virtual address from an instruction
 * fetch or load/store.
 * 
 * Note that you will need to access the per-core icache and dcache. Also note
 * that all caches are physically indexed and physically tagged.
 * 
 * Return the delay in cycles incurred by this memory access. It is intended
 * that the calling function will be responsible for updating the statistics
 * accordingly.
 * 
 * This is intended to be implemented in part D.
 * 
 * @param sys The memory system to use for the access.
 * @param v_line_addr The virtual address of the cache line to access (in units
 *                    of the cache line size, i.e., excluding the line offset
 *                    bits).
 * @param type The type of memory access.
 * @param core_id The CPU core ID that requested this access.
 * @return The delay in cycles incurred by this memory access.
 */
uint64_t memsys_access_modeDEF(MemorySystem *sys, uint64_t v_line_addr,
                               AccessType type, unsigned int core_id)
{
    uint64_t delay = 0;
    uint64_t p_line_addr = 0;
    //NUM_CORES = 2;

    // TODO: First convert lineaddr from virtual (v) to physical (p) using the
    //       function memsys_convert_vpn_to_pfn(). Page size is defined to be
    //       4 KB, as indicated by the PAGE_SIZE constant.
    // Note: memsys_convert_vpn_to_pfn() operates at page granularity and
    //       returns a page number.

    //virtual page number - vpn
    uint64_t vpn = v_line_addr/(PAGE_SIZE/CACHE_LINESIZE);

    //Physical page number - pfn
    uint64_t pfn = memsys_convert_vpn_to_pfn(sys, vpn, core_id);

    //physical line address
    p_line_addr = (pfn * (PAGE_SIZE / CACHE_LINESIZE)) + (v_line_addr % (PAGE_SIZE / CACHE_LINESIZE));

    CacheResult l1_output;

    if (type == ACCESS_TYPE_IFETCH)
    {
        // TODO: Simulate the instruction fetch and update delay accordingly.
        l1_output = cache_access(sys->icache_coreid[core_id], p_line_addr, false, core_id);
        delay += ICACHE_HIT_LATENCY;

        if (l1_output == MISS){
            //accessing L2 cache
            delay += memsys_l2_access(sys, p_line_addr, false, core_id);

            #ifdef DEBUG
                printf("\tInstalling line in L1 cache!\n");
            #endif
            cache_install(sys->icache_coreid[core_id], p_line_addr, false, core_id);
        }
    }
    //if(type == ACCESS_TYPE_LOAD || type == ACCESS_TYPE_STORE)
    else{
        bool is_write = (type == ACCESS_TYPE_STORE);
        l1_output = cache_access(sys->dcache_coreid[core_id], p_line_addr, is_write, core_id);
        delay += DCACHE_HIT_LATENCY;

        if(l1_output == MISS){
            delay += memsys_l2_access(sys, p_line_addr, false, core_id);

            #ifdef DEBUG
                printf("\tInstalling line in L1 cache!\n");
            #endif
            cache_install(sys->dcache_coreid[core_id], p_line_addr, is_write, core_id);
        }

        //Write back to L2 cache - Using Victim Line
        if(sys->dcache_coreid[core_id]->last_evicted_line.valid && sys->dcache_coreid[core_id]->last_evicted_line.dirty && !l1_output){
            uint64_t index_bits = __builtin_log2(sys->dcache_coreid[core_id]->number_of_sets);
            uint64_t ind = extract_index_mem(p_line_addr, index_bits);
            uint64_t evicted_line_address = find_line_address_from_tag_index_mem(sys->dcache_coreid[core_id]->last_evicted_line.tag, ind, index_bits);

            #ifdef DEBUG
                //printf("\tline addr: %ld, tag: %ld, index: %ld\n", line_addr, sys->dcache->last_evicted_line.tag, ind);
                printf("\tEvicted L1 entry was dirty! Performing writeback (addr: %ld)\n", evicted_line_address);
            #endif

            //cache_install(sys->l2cache, evicted_line_address, sys->dcache->last_evicted_line.dirty, core_id);
            memsys_l2_access(sys, evicted_line_address, true, core_id);
        }
    }

    return delay;
}

/**
 * Convert the given virtual page number (VPN) to its corresponding physical
 * frame number (PFN; also known as physical page number, or PPN).
 * 
 * This is implemented for you and shouldn't need to be modified.
 * 
 * Note that you will need additional operations to obtain the VPN from the
 * v_line_addr and to get the physical line_addr using the PFN.
 * 
 * @param sys The memory system being used.
 * @param vpn The virtual page number to convert.
 * @param core_id The CPU core ID that requested this access.
 * @return The physical frame number corresponding to the given VPN.
 */
uint64_t memsys_convert_vpn_to_pfn(MemorySystem *sys, uint64_t vpn,
                                   unsigned int core_id)
{
    assert(NUM_CORES == 2);
    uint64_t tail = vpn & 0x000fffff;
    uint64_t head = vpn >> 20;
    uint64_t pfn = tail + (core_id << 21) + (head << 21);
    return pfn;
}

/**
 * Print the statistics of the memory system.
 * 
 * This is implemented for you. You must not modify its output format.
 * 
 * @param dram The memory system to print the statistics of.
 */
void memsys_print_stats(MemorySystem *sys)
{
    double ifetch_delay_avg = 0;
    double load_delay_avg = 0;
    double store_delay_avg = 0;

    if (sys->stat_ifetch_access)
    {
        ifetch_delay_avg = (double)(sys->stat_ifetch_delay) / (double)(sys->stat_ifetch_access);
    }

    if (sys->stat_load_access)
    {
        load_delay_avg = (double)(sys->stat_load_delay) / (double)(sys->stat_load_access);
    }

    if (sys->stat_store_access)
    {
        store_delay_avg = (double)(sys->stat_store_delay) / (double)(sys->stat_store_access);
    }

    printf("\n");
    printf("MEMSYS_IFETCH_ACCESS   \t\t : %10llu\n", sys->stat_ifetch_access);
    printf("MEMSYS_LOAD_ACCESS     \t\t : %10llu\n", sys->stat_load_access);
    printf("MEMSYS_STORE_ACCESS    \t\t : %10llu\n", sys->stat_store_access);
    printf("MEMSYS_IFETCH_AVGDELAY \t\t : %10.3f\n", ifetch_delay_avg);
    printf("MEMSYS_LOAD_AVGDELAY   \t\t : %10.3f\n", load_delay_avg);
    printf("MEMSYS_STORE_AVGDELAY  \t\t : %10.3f\n", store_delay_avg);

    if (SIM_MODE == SIM_MODE_A)
    {
        cache_print_stats(sys->dcache, "DCACHE");
    }

    if ((SIM_MODE == SIM_MODE_B) || (SIM_MODE == SIM_MODE_C))
    {
        cache_print_stats(sys->icache, "ICACHE");
        cache_print_stats(sys->dcache, "DCACHE");
        cache_print_stats(sys->l2cache, "L2CACHE");
        dram_print_stats(sys->dram);
    }

    if (SIM_MODE == SIM_MODE_DEF)
    {
        assert(NUM_CORES == 2);
        cache_print_stats(sys->icache_coreid[0], "ICACHE_0");
        cache_print_stats(sys->dcache_coreid[0], "DCACHE_0");
        cache_print_stats(sys->icache_coreid[1], "ICACHE_1");
        cache_print_stats(sys->dcache_coreid[1], "DCACHE_1");
        cache_print_stats(sys->l2cache, "L2CACHE");
        dram_print_stats(sys->dram);
    }
}
